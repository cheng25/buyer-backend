/**
 * @file scenario_specific_utils.hpp
 * @brief 场景特定工具集头文件
 * @details 提供媒体附件处理相关的通用工具函数，包括媒体处理、媒体获取等功能。
 * 媒体数据库表结构约定：
 * - 表名格式："prefix"_media（如 offer_media、post_media、message_media）
 * - 外键字段："prefix"_id（如 offer_id、post_id、message_id）
 * - 表前缀："prefix"（如 offer、post、message）
 */
#ifndef SCENARIO_SPECIFIC_UTILS_HPP
#define SCENARIO_SPECIFIC_UTILS_HPP
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>

#include "../services/service_manager.hpp"   // 服务管理器头文件
#include "../utilities/json_manipulation.hpp"  // JSON操作工具头文件
#include "common_req_n_resp.hpp"               // 通用请求与响应结构头文件

/**
 * @brief 快速处理媒体附件（尽力而为）
 * @details 尽力而为的处理可用媒体链接，容忍失败。使用现有数据库事务运行，
 * 处理过程包括对对象键的有效性进行检查，并进行必要的媒体附件插入操作。
 * 即使没有处理完所有媒体，只要没有发生错误就返回true。
 * @param object_keys 媒体对象密钥列表（移动语义）
 * @param transaction 数据库事务对象（需要调用方提供事务）
 * @param current_user_id 当前用户ID
 * @param media_table_prefix 媒体表前缀（如 offer、post、message）
 * @param media_table_prefix_id 关联实体ID（字符串形式）
 * @return Task<bool> true表示处理成功（即使未处理完所有），false表示发生错误
 */
inline drogon::Task<bool> quick_process_media_attachments(
    std::vector<std::string>&& object_keys,
    const std::shared_ptr<drogon::orm::Transaction>& transaction,
    std::string current_user_id, std::string media_table_prefix,
    std::string media_table_prefix_id) {
  for (const auto& object_key : object_keys) {
    MediaInfo info =
        co_await ServiceManager::get_instance().get_s3_service().get_media_info(
            service::BUCKET_NAME, object_key);
    if (info.etag.empty()) {
      LOG_ERROR << "Media info not found for " << object_key;
      continue;
    }

    std::string file_name = object_key.substr(object_key.find('_') + 1);
    std::string mime_type = !info.content_type.empty()
                                ? info.content_type
                                : "application/octet-stream";

    /*
     * 插入媒体数据，插入成功则返回媒体ID
     * 存储键冲突时，将更新文件名、MIME 类型和文件大小
     * EXCLUDED 是一个伪表（pseudo-table），仅在 INSERT ... ON CONFLICT ... DO UPDATE
     * 语句中可用。它代表原本打算插入但因冲突而未能插入的那行数据。
     * 使用 EXCLUDED 引用的值（即原本要插入的 $3, $4, $5）来更新 file_name, mime_type, size
     */
    auto media_result = co_await transaction->execSqlCoro(
        "INSERT INTO media (uploader_id, storage_key, file_name, "
        "mime_type, size) "
        "VALUES ($1, $2, $3, $4, $5) "
        "ON CONFLICT (storage_key) DO UPDATE SET "
        "file_name = EXCLUDED.file_name, "
        "mime_type = EXCLUDED.mime_type, "
        "size = EXCLUDED.size "
        "RETURNING id",
        current_user_id, object_key, file_name, mime_type, info.content_length);

    if (!media_result.empty()) {
      int media_id = media_result[0]["id"].as<int>();
      std::string query =
          std::format("INSERT INTO {}_media ({}_id, media_id) VALUES ($1, $2)",
                      media_table_prefix, media_table_prefix);
      // Link media to particular table 链接媒体到特定的表
      co_await transaction->execSqlCoro(query, media_table_prefix_id, media_id);
    }
  }
  co_return true;
}

/**
 * @brief 完整处理媒体附件（严格模式）
 * @details 完整处理可用媒体，返回处理后的媒体信息。使用现有数据库事务运行，
 * 处理过程包括对对象键的有效性检查并进行必要的媒体附件插入。
 * 参数通过值传递/移动以避免悬空引用。
 * @param object_keys 媒体对象密钥列表（移动语义）
 * @param transaction 数据库事务对象（需要调用方提供事务）
 * @param current_user_id 当前用户ID（整数形式）
 * @param media_table_prefix 媒体表前缀（如 offer、post、message）
 * @param media_table_prefix_id 关联实体ID（整数形式）
 * @return Task<std::expected<std::vector<MediaQuickInfo>, std::string>> 
 * 成功时返回处理后的媒体列表，失败时返回错误信息字符串
 */
inline drogon::Task<std::expected<std::vector<MediaQuickInfo>, std::string>>
process_media_attachments(
    std::vector<std::string>&& object_keys,
    const std::shared_ptr<drogon::orm::Transaction>& transaction,
    int current_user_id, std::string media_table_prefix,
    int media_table_prefix_id) {
  try {
    std::vector<MediaQuickInfo> processed_media;
    processed_media.reserve(object_keys.size());
    for (const auto& object_key : object_keys) {
      MediaInfo info = co_await ServiceManager::get_instance()
                           .get_s3_service()
                           .get_media_info(service::BUCKET_NAME, object_key);
      if (info.etag.empty()) {
        continue;
      }

      std::string filename = object_key.substr(object_key.find('_') + 1);
      std::string mime_type = !info.content_type.empty()
                                  ? info.content_type
                                  : "application/octet-stream";
      //int64_t& size = info.content_length;
      const auto& size = info.content_length;

      /*这是 PostgreSQL 特有的语法，叫做 "UPSERT"（插入或更新） 操作。具体含义是：
ON CONFLICT (storage_key)：当插入数据时，如果 storage_key 字段违反了唯一约束（即该值已存在）
DO UPDATE SET：则执行更新操作，而不是抛出错误
EXCLUDED：这是一个特殊的关键字，代表"尝试插入但被冲突的那行数据"
整体意思：如果 storage_key 已存在，就更新 file_name、mime_type、size 字段为新值；
如果不存在，就插入新记录。这是一种常见的"存在则更新，不存在则插入"的模式。
       */
      auto media_result = co_await transaction->execSqlCoro(
          "INSERT INTO media (uploader_id, storage_key, file_name, "
          "mime_type, size) "
          "VALUES ($1, $2, $3, $4, $5) "
          "ON CONFLICT (storage_key) DO UPDATE SET "
          "file_name = EXCLUDED.file_name, "
          "mime_type = EXCLUDED.mime_type, "
          "size = EXCLUDED.size "
          "RETURNING id",
          current_user_id, object_key, filename, mime_type, size);

      if (!media_result.empty()) {
        auto media_id = media_result[0]["id"].as<int>();
        std::string query = std::format(
            "INSERT INTO {}_media ({}_id, media_id) VALUES ($1, $2)",
            media_table_prefix, media_table_prefix);
        // Link media to particular table// 链接媒体到特定的表
        co_await transaction->execSqlCoro(query, media_table_prefix_id,
                                          media_id);

        processed_media.emplace_back(MediaQuickInfo{.media_id = media_id,
                                                    .object_key = object_key,
                                                    .filename = filename,
                                                    .mime_type = mime_type,
                                                    .size = size});

        // Alternatively Generate presigned URL for viewing
        // try {
        //   auto presigned_url = co_await ServiceManager::get_instance()
        //                         .get_s3_service()
        //                         .generate_presigned_url(service::BUCKET_NAME,
        //                         object_key,
        //                         drogon::HttpMethod::Get,
        //                         mime_type);
        //   processed_media.emplace_back(MediaQuickInfo{.media_id = media_id,
        //       .object_key = object_key,
        //       .filename = filename,
        //       .mime_type = mime_type,
        //       .size = size,
        //       .presigned_url = presigned_url});
        // } catch (const std::exception& e) {
        //   LOG_ERROR << "Failed to generate presigned URL: " << e.what();
        // processed_media.emplace_back(MediaQuickInfo{.media_id = media_id,
        //                                             .object_key = object_key,
        //                                             .filename = filename,
        //                                             .mime_type = mime_type,
        //                                             .size = size});
        // }
      }
    }
    co_return processed_media;

  } catch (const drogon::orm::DrogonDbException& e) {
    LOG_ERROR << "Database error: " << e.base().what();
    co_return std::unexpected(e.base().what());
  } catch (const std::exception& e) {
    LOG_ERROR << "Database error: " << e.what();
    co_return std::unexpected(e.what());
  }
}

/**
 * @brief 完整处理媒体附件并返回响应
 * @details 完整处理可用媒体并直接返回HTTP响应给客户端。使用现有数据库事务运行，
 * 检查对象键的有效性，进行必要的媒体附件插入操作，并向客户端返回响应。
 * 如果部分媒体处理失败，则回滚事务并返回错误。
 * @param callback HTTP响应回调函数
 * @param object_keys 媒体对象密钥列表（移动语义）
 * @param transaction 数据库事务对象（需要调用方提供事务）
 * @param current_user_id 当前用户ID（字符串形式）
 * @param media_table_prefix 媒体表前缀（如 offer、post、message）
 * @param media_table_prefix_id 关联实体ID（字符串形式）
 * @return Task<> 异步任务
 */
inline drogon::Task<> process_media_attachments_with_response(
    std::function<void(const drogon::HttpResponsePtr&)> callback,
    std::vector<std::string>&& object_keys,
    const std::shared_ptr<drogon::orm::Transaction>& transaction,
    std::string current_user_id, std::string media_table_prefix,
    std::string media_table_prefix_id) {
  try {
    MediaResponse media_resp;
    media_resp.media_ids.reserve(object_keys.size());
    auto& processed_media = media_resp.media_ids;
    for (const auto& object_key : object_keys) {
      MediaInfo info = co_await ServiceManager::get_instance()
                           .get_s3_service()
                           .get_media_info(service::BUCKET_NAME, object_key);
      if (info.etag.empty()) {
        continue;
      }

      std::string filename = object_key.substr(object_key.find('_') + 1);
      std::string mime_type = !info.content_type.empty()
                                  ? info.content_type
                                  : "application/octet-stream";
      //int64_t& size = info.content_length;
      const auto& size = info.content_length;

      auto media_result = co_await transaction->execSqlCoro(
          "INSERT INTO media (uploader_id, storage_key, file_name, "
          "mime_type, size) "
          "VALUES ($1, $2, $3, $4, $5) "
          "ON CONFLICT (storage_key) DO UPDATE SET "
          "file_name = EXCLUDED.file_name, "
          "mime_type = EXCLUDED.mime_type, "
          "size = EXCLUDED.size "
          "RETURNING id",
          current_user_id, object_key, filename, mime_type, size);

      if (!media_result.empty()) {
        auto media_id = media_result[0]["id"].as<int>();
        std::string query = std::format(
            "INSERT INTO {}_media ({}_id, media_id) VALUES ($1, $2)",
            media_table_prefix, media_table_prefix);
        // Link media to particular table
        co_await transaction->execSqlCoro(query, media_table_prefix_id,
                                          media_id);

        processed_media.emplace_back(media_id);
      }
    }

    if (processed_media.size() < object_keys.size()) {
      LOG_ERROR << " Some Media info was not found";
      transaction->rollback();
      std::string error_string;
      for (const auto& name : processed_media) {
        error_string += std::format("{},", name);
      }
      SimpleError error{.error =
                            std::format("Media info not found or processed, "
                                        "only the following media items "
                                        "were processed:\n{}",
                                        error_string)};
      auto resp = drogon::HttpResponse::newHttpResponse(
          drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k200OK, drogon::CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(media_resp).value_or(""));
    callback(resp);
    co_return;

  } catch (const drogon::orm::DrogonDbException& e) {
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError error{.error = "error during processing"};
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k500InternalServerError, drogon::CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
    co_return;
  } catch (const std::exception& e) {
    LOG_ERROR << "Error: " << e.what();
    SimpleError error{.error = "error during processing"};
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k500InternalServerError, drogon::CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
    co_return;
  }
}

/**
 * @brief 获取媒体附件
 * @details 根据媒体表前缀和关联实体ID查询媒体附件信息。通过内连接查询关联表和媒体表，
 * 获取媒体的详细信息（ID、存储键、文件名、MIME类型、大小等）。
 * 参数通过值传递以避免悬空引用。
 * @param media_table_prefix 媒体表前缀（如 offer、post、message）
 * @param media_table_prefix_id 关联实体ID（整数形式）
 * @return Task<std::expected<std::vector<MediaQuickInfo>, std::string>> 
 * 成功时返回媒体列表，失败时返回错误信息字符串
 */
inline drogon::Task<std::expected<std::vector<MediaQuickInfo>, std::string>>
get_media_attachments(std::string media_table_prefix,
                      int media_table_prefix_id) {
  auto db = drogon::app().getDbClient();
  try {
    /*
     * 内连接：xxx_media 与 media 共同交集
     */
    auto media_result = co_await db->execSqlCoro(
        std::format(
            "SELECT med.id, med.storage_key, med.file_name, med.mime_type, "
            "med.size, med.metadata "
            "FROM {}_media om "
            "INNER JOIN media med ON om.media_id = med.id "
            "WHERE om.{}_id = $1",  // ORDER BY med.created_at DESC
            media_table_prefix, media_table_prefix),
        media_table_prefix_id);

    std::vector<MediaQuickInfo> media_array;
    if (!media_result.empty()) {
      media_array.reserve(media_result.size());

      for (const auto& media_row : media_result) {
        media_array.emplace_back(MediaQuickInfo{
            .media_id = media_row["id"].as<int>(),
            .object_key = media_row["storage_key"].as<std::string>(),
            .filename = media_row["file_name"].as<std::string>(),
            .mime_type = media_row["mime_type"].as<std::string>(),
            .size = media_row["size"].as<int64_t>()});
        // alternatively Generate presigned URL for viewing
        // std::string object_key =
        // media_row["storage_key"].as<std::string>();
        // std::string mime_type = media_row["mime_type"].as<std::string>();

        // try {
        //   auto presigned_url = co_await ServiceManager::get_instance()
        //                        .get_s3_service()
        //                        .generate_presigned_url(service::BUCKET_NAME,
        //                         object_key,
        //                         drogon::HttpMethod::Get,
        //                         mime_type);
        //   media_item["presigned_url"] = presigned_url;
        // } catch (const std::exception& e) {
        //   LOG_ERROR << "Failed to generate presigned URL: " << e.what();
        //   media_item["presigned_url"] = "";
        // }
      }
    }
    co_return media_array;
  } catch (const drogon::orm::DrogonDbException& e) {
    LOG_ERROR << std::format("Database error: getting {} media: {}",
                             media_table_prefix, e.base().what());
    co_return std::unexpected(e.base().what());
  } catch (const std::exception& e) {
    LOG_ERROR << std::format("Error getting {} media: {}", media_table_prefix,
                             e.what());
    co_return std::unexpected(e.what());
  }
}

/**
 * @brief 获取媒体附件并返回响应
 * @details 根据媒体表前缀和关联实体ID查询媒体附件信息，并直接返回HTTP响应给客户端。
 * 通过内连接查询关联表和媒体表，获取媒体的详细信息。适用于简单GET端点的直接响应。
 * 参数通过值传递以避免悬空引用。
 * @param callback HTTP响应回调函数
 * @param media_table_prefix 媒体表前缀（如 offer、post、message）
 * @param media_table_prefix_id 关联实体ID（字符串形式）
 * @return Task<> 异步任务
 */
inline drogon::Task<> get_media_attachments_with_response(
    std::function<void(const drogon::HttpResponsePtr&)> callback,
    std::string media_table_prefix, std::string media_table_prefix_id) {
  auto db = drogon::app().getDbClient();
  try {
    auto media_result = co_await db->execSqlCoro(
        std::format(
            "SELECT med.id, med.storage_key, med.file_name, med.mime_type, "
            "med.size, med.metadata "
            "FROM {}_media om "
            "INNER JOIN media med ON om.media_id = med.id "
            "WHERE om.{}_id = $1",  // ORDER BY med.created_at DESC
            media_table_prefix, media_table_prefix),
        media_table_prefix_id);
    MediaInfoResponse media_resp;
    if (!media_result.empty()) {
      auto& media_array = media_resp.media;
      media_array.reserve(media_result.size());

      for (const auto& media_row : media_result) {
        media_array.emplace_back(MediaQuickInfo{
            .media_id = media_row["id"].as<int>(),
            .object_key = media_row["storage_key"].as<std::string>(),
            .filename = media_row["file_name"].as<std::string>(),
            .mime_type = media_row["mime_type"].as<std::string>(),
            .size = media_row["size"].as<int64_t>()});
        // alternatively Generate presigned URL for viewing
        // std::string object_key =
        // media_row["storage_key"].as<std::string>();
        // std::string mime_type = media_row["mime_type"].as<std::string>();

        // try {
        //   auto presigned_url = co_await ServiceManager::get_instance()
        //                        .get_s3_service()
        //                        .generate_presigned_url(service::BUCKET_NAME,
        //                         object_key,
        //                         drogon::HttpMethod::Get,
        //                         mime_type);
        //   media_item["presigned_url"] = presigned_url;
        // } catch (const std::exception& e) {
        //   LOG_ERROR << "Failed to generate presigned URL: " << e.what();
        //   media_item["presigned_url"] = "";
        // }
      }
    }
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k200OK, drogon::CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(media_resp).value_or(""));
    callback(resp);
    co_return;
  } catch (const drogon::orm::DrogonDbException& e) {
    LOG_ERROR << std::format("Database error: getting {} media: {}",
                             media_table_prefix, e.base().what());
    SimpleError error{.error = "error during fetch"};
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k500InternalServerError, drogon::CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
    co_return;
  } catch (const std::exception& e) {
    LOG_ERROR << std::format("Error getting {} media: {}", media_table_prefix,
                             e.what());
    SimpleError error{.error = "error during fetch"};
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k500InternalServerError, drogon::CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
    co_return;
  }
}

#endif  // SCENARIO_SPECIFIC_UTILS_HPP
