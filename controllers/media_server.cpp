/**
 * @file media_server.cpp
 * @brief 媒体服务器控制器实现文件
 * @details 实现媒体文件上传URL生成、文件验证、元数据获取、证明验证和下载URL生成等功能。
 */
// clang-format off
#include "../utilities/uuid_generator.hpp"
// clang-format on

#include "media_server.hpp"

#include <array>
#include <format>
#include <optional>
#include <string>

#include "../services/media_server/s3_service.hpp"
#include "../services/service_manager.hpp"
#include "../utilities/json_manipulation.hpp"
#include "common_req_n_resp.hpp"

using api::v1::MediaController;
using drogon::CT_APPLICATION_JSON;

/**
 * @struct GetUploadUrlRequest
 * @brief 获取上传URL请求数据结构
 * @details 包含文件名、是否为证明文件和内容类型等参数。
 */
struct GetUploadUrlRequest {
  std::string filename;                          // 文件名
  std::optional<bool> is_proof{false};           // 是否为证明文件，默认false
  std::optional<std::string> content_type{"application/octet-stream"};  // 内容类型
};

/**
 * @struct GetUploadUrlResponse
 * @brief 获取上传URL响应数据结构
 * @details 返回预签名上传URL和对象键。
 */
struct GetUploadUrlResponse {
  std::string upload_url;                        // 预签名上传URL
  std::string object_key;                        // 对象键
};

/**
 * @struct VerifyObjectKeyResponse
 * @brief 验证对象键响应数据结构
 * @details 返回对象是否存在。
 */
struct VerifyObjectKeyResponse {
  bool exists;                                   // 对象是否存在
};

/**
 * @struct GetMediaMetadataResponse
 * @brief 获取媒体元数据响应数据结构
 * @details 返回媒体文件的完整元数据信息。
 */
struct GetMediaMetadataResponse {
  std::string object_key;                        // 对象键
  std::string content_type;                      // 内容类型
  int64_t content_length;                        // 内容长度（字节）
  std::string last_modified;                     // 最后修改时间
  std::string etag;                              // ETag标识符
};

/**
 * @struct VerifyProofResponse
 * @brief 验证证明响应数据结构
 * @details 返回证明是否有效和对象键。
 */
struct VerifyProofResponse {
  bool is_valid;                                 // 证明是否有效
  std::string object_key;                        // 对象键
};

/**
 * @struct GetMediaUrlResponse
 * @brief 获取媒体URL响应数据结构
 * @details 返回下载URL和内容类型。
 */
struct GetMediaUrlResponse {
  std::string download_url;                      // 下载URL
  std::string content_type;                      // 内容类型
};

/**
 * @brief 允许上传的文件类型列表
 * @details 限制仅允许图片和视频类型的文件上传。
 */
constexpr auto allowed_file_types = std::to_array<std::string_view>({
    "image/jpeg", "image/png", "image/gif", "image/webp", "video/mp4",
    "video/webm",  // "video/quicktime",
});

/**
 * @brief 获取媒体文件上传URL
 * @details 生成S3预签名上传URL，用于客户端直接上传媒体文件。支持文件类型白名单校验。
 * @param req HTTP请求指针，包含filename、可选的is_proof和content_type参数
 * @param callback 响应回调函数
 * @return Task协程对象
 */
drogon::Task<> MediaController::get_upload_url(
    const drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  try {
    GetUploadUrlRequest upload_req;
    auto parse_error = utilities::strict_read_json(upload_req, req->getBody());

    if (parse_error || upload_req.filename.empty()) {
      LOG_ERROR
          << "At least filename is required, is_proof should be a boolean";
      SimpleError error{
          .error =
              "At least filename is required, is_proof should be a "
              "boolean"};
      auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest,
                                                        CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    auto content_type =
        upload_req.content_type.value_or("application/octet-stream");

    if (std::find(allowed_file_types.begin(), allowed_file_types.end(),
                  content_type) == allowed_file_types.end()) {
      LOG_ERROR << content_type << " file type not allowed";
      SimpleError error{
          .error = std::format("{} file type not allowed", content_type)};
      auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest,
                                                        CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    auto object_key = std::format(
        "uploads/{}_{}", UuidGenerator::generate_uuid(), upload_req.filename);

    auto upload_url = co_await ServiceManager::get_instance()
                          .get_s3_service()
                          .generate_presigned_url("media", object_key,
                                                  drogon::Put, content_type);

    if (upload_url.empty()) {
      LOG_ERROR << "Failed to generate upload url";
      SimpleError error{.error = "Failed to generate upload url"};
      auto resp = drogon::HttpResponse::newHttpResponse(
          drogon::k500InternalServerError, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    GetUploadUrlResponse response{.upload_url = upload_url,
                                  .object_key = object_key};

    auto resp = drogon::HttpResponse::newHttpResponse(drogon::k200OK,
                                                      CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(response).value_or(""));
    callback(resp);

  } catch (const std::exception &e) {
    LOG_ERROR << "Failed to get upload url: " << e.what();
    SimpleError error{.error = e.what()};
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k500InternalServerError, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }
  co_return;
}

/**
 * @brief 验证对象键是否存在
 * @details 检查指定的object_key在S3存储桶中是否存在。
 * @param req HTTP请求指针，支持object_key查询参数
 * @param callback 响应回调函数
 * @return Task协程对象
 */
drogon::Task<> MediaController::verify_object_key(
    const drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  try {
    auto object_key = req->getParameter("object_key");

    if (object_key.empty()) {
      LOG_ERROR << "object_key is required";
      SimpleError error{.error = "object_key is required"};
      auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest,
                                                        CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    MediaInfo info =
        co_await ServiceManager::get_instance().get_s3_service().get_media_info(
            "media", object_key);

    VerifyObjectKeyResponse response{.exists =
                                         info.etag.empty() ? false : true};

    auto resp = drogon::HttpResponse::newHttpResponse(drogon::k200OK,
                                                      CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(response).value_or(""));
    callback(resp);
  } catch (const std::exception &e) {
    LOG_ERROR << "Failed confirm media: " << e.what();
    SimpleError error{.error = e.what()};
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k500InternalServerError, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }
  co_return;
}

/**
 * @brief 获取媒体文件元数据
 * @details 获取指定object_key对应的媒体文件的完整元数据信息，包括内容类型、大小、修改时间等。
 * @param req HTTP请求指针，支持object_key查询参数
 * @param callback 响应回调函数
 * @return Task协程对象
 */
drogon::Task<> MediaController::get_media_metadata(
    const drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  try {
    auto object_key = req->getParameter("object_key");

    if (object_key.empty()) {
      LOG_ERROR << "object_key is required";
      SimpleError error{.error = "object_key is required"};
      auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest,
                                                        CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    MediaInfo info =
        co_await ServiceManager::get_instance().get_s3_service().get_media_info(
            "media", object_key);

    GetMediaMetadataResponse response{.object_key = info.object_key,
                                      .content_type = info.content_type,
                                      .content_length = info.content_length,
                                      .last_modified = info.last_modified,
                                      .etag = info.etag};

    auto resp = drogon::HttpResponse::newHttpResponse(drogon::k200OK,
                                                      CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(response).value_or(""));
    callback(resp);
  } catch (const std::exception &e) {
    LOG_ERROR << "Failed confirm media: " << e.what();
    SimpleError error{.error = e.what()};
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k500InternalServerError, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }
  co_return;
}

/**
 * @brief 验证证明文件
 * @details 处理并验证指定的证明文件是否有效。
 * @param req HTTP请求指针，支持object_key查询参数
 * @param callback 响应回调函数
 * @return Task协程对象
 */
drogon::Task<> MediaController::verify_proof(
    const drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  try {
    auto object_key = req->getParameter("object_key");

    if (object_key.empty()) {
      LOG_ERROR << "object_key is required";
      SimpleError error{.error = "object_key is required"};
      auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest,
                                                        CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    bool is_valid =
        co_await ServiceManager::get_instance().get_s3_service().process_media(
            "media", object_key);

    VerifyProofResponse response{.is_valid = is_valid,
                                 .object_key = object_key};

    auto resp = drogon::HttpResponse::newHttpResponse(drogon::k200OK,
                                                      CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(response).value_or(""));
    callback(resp);
  } catch (const std::exception &e) {
    LOG_ERROR << "Failed verify proof: " << e.what();
    SimpleError error{.error = e.what()};
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k500InternalServerError, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }
  co_return;
}

/**
 * @brief 获取媒体文件下载URL
 * @details 生成S3预签名下载URL，根据文件扩展名自动推断内容类型。
 * @param req HTTP请求指针，支持object_key查询参数
 * @param callback 响应回调函数
 * @return Task协程对象
 */
drogon::Task<> MediaController::get_media_url(
    const drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  try {
    auto object_key = req->getParameter("object_key");

    if (object_key.empty()) {
      SimpleError error{.error = "object_key is required as a query parameter"};
      auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest,
                                                        CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    std::string content_type = "application/octet-stream";
    if (object_key.ends_with(".png")) {
      content_type = "image/png";
    } else if (object_key.ends_with(".jpg") || object_key.ends_with(".jpeg")) {
      content_type = "image/jpeg";
    } else if (object_key.ends_with(".mp4")) {
      content_type = "video/mp4";
    } else if (object_key.ends_with(".webm")) {
      content_type = "video/webm";
    } else if (object_key.ends_with(".mp3")) {
      content_type = "audio/mpeg";
    } else if (object_key.ends_with(".wav")) {
      content_type = "audio/wav";
    } else if (object_key.ends_with(".pdf")) {
      content_type = "application/pdf";
    }

    auto &s3_service = ServiceManager::get_instance().get_s3_service();

    auto view_url = co_await s3_service.generate_presigned_url(
        "media", object_key, drogon::HttpMethod::Get);

    if (view_url.empty()) {
      LOG_ERROR << "object url not found";
      SimpleError error{.error = "object url not found"};
      auto resp = drogon::HttpResponse::newHttpResponse(drogon::k404NotFound,
                                                        CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    GetMediaUrlResponse response{.download_url = view_url,
                                 .content_type = content_type};
    auto resp = drogon::HttpResponse::newHttpResponse(drogon::k200OK,
                                                      CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(response).value_or(""));
    callback(resp);
  } catch (const std::exception &e) {
    LOG_ERROR << "Failed to get media: " << e.what();
    SimpleError error{.error = "Failed to retrieve media"};
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k500InternalServerError, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }
  co_return;
}
