/**
 * @file chats.cc
 * @brief 聊天控制器实现文件
 * @details 实现对话管理、消息发送与接收、消息阅读状态更新和未读消息计数等聊天功能。
 */

#include "chats.hpp"                              // 引入聊天控制器头文件

#include <drogon/HttpResponse.h>                  // 引入Drogon HTTP响应头文件
#include <drogon/HttpTypes.h>                     // 引入Drogon HTTP类型头文件
#include <drogon/orm/Criteria.h>                  // 引入Drogon ORM条件头文件
#include <drogon/orm/DbClient.h>                  // 引入Drogon数据库客户端头文件
#include <drogon/orm/Exception.h>                // 引入Drogon数据库异常头文件
#include <drogon/orm/Field.h>                     // 引入Drogon ORM字段头文件
#include <drogon/orm/Mapper.h>                    // 引入Drogon ORM映射器头文件
#include <drogon/orm/Result.h>                    // 引入Drogon ORM结果头文件
#include <drogon/orm/ResultIterator.h>            // 引入Drogon ORM结果迭代器头文件
#include <drogon/orm/Row.h>                       // 引入Drogon ORM行头文件
#include <drogon/orm/SqlBinder.h>                 // 引入Drogon ORM SQL绑定器头文件

#include <format>                                 // 引入格式化输出头文件

#include "../services/service_manager.hpp"        // 引入服务管理器头文件
#include "../utilities/conversion.hpp"            // 引入类型转换工具头文件
#include "../utilities/json_manipulation.hpp"     // 引入JSON操作工具头文件
#include "common_req_n_resp.hpp"                  // 引入通用请求响应结构头文件
#include "scenario_specific_utils.hpp"            // 引入场景特定工具头文件

using drogon::app;                                // Drogon应用实例别名
using drogon::CT_APPLICATION_JSON;                // JSON内容类型别名
using drogon::HttpRequestPtr;                     // HTTP请求指针别名
using drogon::HttpResponse;                       // HTTP响应类别名
using drogon::HttpResponsePtr;                    // HTTP响应指针别名
using drogon::k200OK;                             // 200成功码别名
using drogon::k400BadRequest;                     // 400错误码别名
using drogon::k401Unauthorized;                   // 401错误码别名
using drogon::k403Forbidden;                      // 403错误码别名
using drogon::k404NotFound;                       // 404错误码别名
using drogon::k409Conflict;                       // 409错误码别名
using drogon::k500InternalServerError;            // 500错误码别名
using drogon::Task;                               // Drogon协程任务别名
using drogon::orm::DrogonDbException;             // 数据库异常类别名

using api::v1::Chats;                             // 聊天控制器类别名

/**
 * @struct Conversation
 * @brief 对话信息结构体
 * @details 存储对话的基本信息，包括ID、名称、对方用户名、最后一条消息和修改时间。
 */
struct Conversation {
  int id;                          // 对话ID
  std::string name;                // 对话名称
  std::string other_username;      // 对方用户名
  std::string lastMessage;         // 最后一条消息内容
  std::string modified_at;         // 修改时间
};

/**
 * @struct CreateConversationRequest
 * @brief 创建对话请求结构体
 * @details 存储创建对话时所需的参数：对话名称和对方用户ID。
 */
struct CreateConversationRequest {
  std::string name;                // 对话名称
  int user_id;                     // 对方用户ID
};

/**
 * @struct CreateConversationResponse
 * @brief 创建对话响应结构体
 * @details 存储创建对话后的响应信息：状态、对话ID和消息。
 */
struct CreateConversationResponse {
  std::string status;              // 状态
  int conversation_id;             // 对话ID
  std::string message;             // 消息
};

/**
 * @struct Message
 * @brief 消息结构体
 * @details 存储消息的详细信息，包括发送者、内容、类型、阅读状态和媒体附件。
 */
struct Message {
  int id;                          // 消息ID
  int sender_id;                   // 发送者ID
  std::string sender_name;         // 发送者名称
  std::string content;             // 消息内容
  std::string message_type;        // 消息类型（text/media/mixed）
  bool is_read;                    // 是否已读
  std::string created_at;          // 创建时间
  std::string metadata;            // JSON格式的元数据
  std::optional<std::vector<MediaQuickInfo>> media;  // 媒体附件列表
};

/**
 * @struct SendMessageRequest
 * @brief 发送消息请求结构体
 * @details 存储发送消息时所需的参数：消息内容和媒体文件列表。
 */
struct SendMessageRequest {
  std::optional<std::string> content;            // 消息内容（可选）
  std::optional<std::vector<std::string>> media; // 媒体文件列表（可选）
};

/**
 * @struct SendMessageResponse
 * @brief 发送消息响应结构体
 * @details 存储发送消息后的响应信息：状态、消息ID、创建时间、消息类型和媒体信息。
 */
struct SendMessageResponse {
  std::string status;                            // 状态
  int message_id;                                // 消息ID
  std::string created_at;                        // 创建时间
  std::string message_type;                      // 消息类型
  std::optional<std::vector<MediaQuickInfo>> media;  // 媒体附件列表
};

/**
 * @struct GetConversationByOfferResponse
 * @brief 通过报价获取对话响应结构体
 * @details 存储通过报价ID获取对话后的响应信息：状态、对话ID和是否为新对话。
 */
struct GetConversationByOfferResponse {
  std::string status;              // 状态
  int conversation_id;             // 对话ID
  bool is_new;                     // 是否为新创建的对话
};

/**
 * @struct MarkMessagesAsReadResponse
 * @brief 标记消息已读响应结构体
 * @details 存储标记消息已读后的响应信息：状态和标记的消息数量。
 */
struct MarkMessagesAsReadResponse {
  std::string status;              // 状态
  int messages_marked;             // 标记为已读的消息数量
};

/**
 * @struct UnreadCountResponse
 * @brief 未读消息数量响应结构体
 * @details 存储用户未读消息的总数。
 */
struct UnreadCountResponse {
  int unread_count;                // 未读消息数量
};

/**
 * @brief 获取用户所有对话列表
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @details 查询当前用户参与的所有对话，包含对方用户名和最后一条消息信息。
 */
Task<> Chats::get_conversations(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback) {
  std::string user_id =
      req->getAttributes()->get<std::string>("current_user_id");    // 获取当前用户ID

  auto db = app().getDbClient();    // 获取数据库客户端

  try {
    // Get all conversations where the current user is a participant
    // Include the latest message and other participant info
    // 获取当前用户参与的所有对话，包含最新消息和对方参与者信息
    auto result = co_await db->execSqlCoro(
        "SELECT c.id, c.name, c.created_at, "
        "COALESCE(u.username, 'Unknown') as other_username, "
        "COALESCE(m.content, '') as last_message, "
        "COALESCE(m.created_at, c.created_at) as last_message_time "
        "FROM conversations c "
        "JOIN conversation_participants cp ON c.id = cp.conversation_id "
        "LEFT JOIN conversation_participants cp2 ON c.id = cp2.conversation_id "
        "AND cp2.user_id != $1 "
        "LEFT JOIN users u ON cp2.user_id = u.id "
        "LEFT JOIN ( "
        "  SELECT conversation_id, content, created_at, "
        "  ROW_NUMBER() OVER (PARTITION BY conversation_id ORDER BY created_at "
        "DESC) as rn "
        "  FROM messages "
        ") m ON m.conversation_id = c.id AND m.rn = 1 "
        "WHERE cp.user_id = $1 "
        "ORDER BY last_message_time DESC",
        convert::string_to_int(user_id).value());    // 执行SQL查询

    std::vector<Conversation> data;
    data.reserve(result.size());    // 预分配空间
    for (const auto& row : result) {
      data.emplace_back(Conversation{
          .id = row["id"].as<int>(),
          .name = row["name"].as<std::string>(),
          .other_username = row["other_username"].as<std::string>(),
          .lastMessage = row["last_message"].as<std::string>(),
          .modified_at = row["created_at"].as<std::string>()});    // 构建对话对象
    }

    auto resp = HttpResponse::newHttpResponse(k200OK, CT_APPLICATION_JSON);    // 返回200成功
    resp->setBody(glz::write_json(data).value_or(""));    // 设置响应体
    callback(resp);
  } catch (const DrogonDbException& e) {    // 数据库异常处理
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError ret{.error = "Database error"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);    // 返回500错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 创建对话
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @details 创建新的对话，添加参与者，并通知相关用户。如果对话已存在则返回现有对话ID。
 */
Task<> Chats::create_conversation(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback) {
  auto body = req->getBody();    // 获取请求体
  CreateConversationRequest create_conv_req;
  auto parse_error = utilities::strict_read_json(create_conv_req, body);    // 解析JSON请求体

  if (parse_error || create_conv_req.name.empty() ||
      create_conv_req.user_id < 0) {    // 验证请求参数
    LOG_INFO << "Validation failed for create_conversation";
    SimpleError ret{.error = "Valid user_id and name required"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);    // 返回400错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
    co_return;
  }

  int other_user_id = create_conv_req.user_id;    // 获取对方用户ID
  std::string name = create_conv_req.name;        // 获取对话名称
  std::string user_id =
      req->getAttributes()->get<std::string>("current_user_id");    // 获取当前用户ID

  auto db = app().getDbClient();    // 获取数据库客户端

  try {
    // 检查对话是否已存在
    auto result = co_await db->execSqlCoro(
        "SELECT c.id FROM conversations c "
        "JOIN conversation_participants cp1 ON c.id = cp1.conversation_id AND "
        "cp1.user_id = $1 "
        "JOIN conversation_participants cp2 ON c.id = cp2.conversation_id AND "
        "cp2.user_id = $2 "
        "LIMIT 1",
        convert::string_to_int(user_id).value(), other_user_id);    // 执行SQL查询

    if (!result.empty()) {    // 对话已存在
      CreateConversationResponse ret{
          .status = "success",
          .conversation_id = result[0]["id"].as<int>(),
          .message = "Conversation already exists"};
      auto resp = HttpResponse::newHttpResponse(k200OK, CT_APPLICATION_JSON);    // 返回200成功
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    } else {    // 对话不存在，创建新对话
      auto insert_result = co_await db->execSqlCoro(
          "INSERT INTO conversations (name) VALUES ($1) RETURNING id, "
          "created_at",
          name);    // 插入对话记录

      if (insert_result.empty()) {    // 插入失败
        SimpleError ret{.error = "Failed to create conversation"};
        auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                  CT_APPLICATION_JSON);    // 返回500错误
        resp->setBody(glz::write_json(ret).value_or(""));
        callback(resp);
        co_return;
      }
      int conversation_id = insert_result[0]["id"].as<int>();    // 获取新对话ID
      std::string conversation_id_str =
          insert_result[0]["id"].as<std::string>();

      // Add participants 添加参与者
      try {
        co_await db->execSqlCoro(
            "INSERT INTO conversation_participants (conversation_id, "
            "user_id) VALUES ($1, $2), ($1, $3)",
            conversation_id, convert::string_to_int(user_id).value(),
            other_user_id);    // 插入参与者记录

        // notification 发送通知
        std::string conversation_topic =
            create_topic("chat", conversation_id_str);    // 创建通知主题
        std::string other_user_id_str = std::to_string(other_user_id);
        ServiceManager::get_instance().get_subscriber().subscribe(
            conversation_topic);    // 订阅主题
        ServiceManager::get_instance().get_connection_manager().subscribe(
            conversation_topic, user_id);    // 订阅连接管理器主题
        ServiceManager::get_instance().get_connection_manager().subscribe(
            conversation_topic, other_user_id_str);    // 订阅对方用户主题
        store_user_subscription(user_id, conversation_topic);    // 存储用户订阅
        store_user_subscription(other_user_id_str, conversation_topic);    // 存储对方用户订阅
        LOG_INFO << "User " << user_id << " and " << other_user_id_str
                 << " subscribed to conversation topic: " << conversation_topic;

        NotificationMessage msg{
            .type = "chat_created",
            .id = conversation_id_str,
            .message = "New Conversation created",
            .modified_at = insert_result[0]["created_at"].as<std::string>()};    // 构建通知消息

        ServiceManager::get_instance().get_publisher().publish(
            conversation_topic, glz::write_json(msg).value_or(""));    // 发布通知

        CreateConversationResponse ret{.status = "success",
                                       .conversation_id = conversation_id,
                                       .message = {}};    // 构建响应
        auto resp = HttpResponse::newHttpResponse(k200OK, CT_APPLICATION_JSON);    // 返回200成功
        resp->setBody(glz::write_json(ret).value_or(""));    // 设置响应体
        callback(resp);
      } catch (const DrogonDbException& e) {    // 添加参与者失败
        LOG_ERROR << "Database error adding participants: " << e.base().what();
        SimpleError ret{.error = "Database error"};
        auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                  CT_APPLICATION_JSON);    // 返回500错误
        resp->setBody(glz::write_json(ret).value_or(""));
        callback(resp);
      }
    }
  } catch (const DrogonDbException& e) {    // 检查对话存在性失败
    LOG_ERROR << "Database error checking existing conversation: "
              << e.base().what();
    SimpleError ret{.error = "Database error"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);    // 返回500错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 获取对话消息列表
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @param conversation_id 对话ID
 * @details 查询指定对话的所有消息，验证用户权限，并获取媒体附件信息。
 */
Task<> Chats::get_messages(HttpRequestPtr req,
                           std::function<void(const HttpResponsePtr&)> callback,
                           std::string conversation_id) {
  std::string user_id =
      req->getAttributes()->get<std::string>("current_user_id");    // 获取当前用户ID

  auto conv_id_optional = convert::string_to_int(conversation_id);    // 转换对话ID
  if (!conv_id_optional || conv_id_optional.value() < 0) {    // 验证对话ID有效性
    SimpleError ret{.error = "Invalid conversation_id"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);    // 返回400错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
    co_return;
  }
  int conv_id = conv_id_optional.value();    // 获取对话ID
  auto db = app().getDbClient();    // 获取数据库客户端

  try {
    // 验证用户是否为对话参与者
    auto result = co_await db->execSqlCoro(
        "SELECT 1 FROM conversation_participants WHERE conversation_id = $1 "
        "AND "
        "user_id = $2",
        conv_id, convert::string_to_int(user_id).value());    // 执行权限验证

    if (result.empty()) {    // 用户不是对话参与者
      SimpleError ret{.error = "Unauthorized access to conversation"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);    // 返回403错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }

    // 查询对话消息列表
    auto messages_result = co_await db->execSqlCoro(
        "SELECT m.id, m.sender_id, u.username as sender_name, m.content, "
        "m.message_type, m.is_read, m.created_at, m.metadata "
        "FROM messages m "
        "JOIN users u ON m.sender_id = u.id "
        "WHERE m.conversation_id = $1 "
        "ORDER BY m.created_at ASC",
        conv_id);    // 执行消息查询

    std::vector<Message> messages_list;
    messages_list.reserve(messages_result.size());    // 预分配空间
    for (const auto& row : messages_result) {
      int message_id = row["id"].as<int>();    // 获取消息ID

#if 0
      //See <file:///usr/share/doc/gcc-14/README.Bugs> for instructions.
      auto media_attachments =
          row["message_type"].as<std::string>() != "text"
              ? co_await get_media_attachments("message", message_id)
              : std::unexpected<std::string>("failed");
#else
      // ============== 安全改写版本 ==============
      std::optional<std::vector<MediaQuickInfo>> media_attachments;
      if (row["message_type"].as<std::string>() != "text") {    // 如果不是文本消息，获取媒体附件
        auto res = co_await get_media_attachments("message", message_id);
        media_attachments = res.value_or(std::vector<MediaQuickInfo>());
      } else {
        media_attachments = {};    // 文本消息无媒体附件
      }
# endif
      messages_list.emplace_back(
          Message{.id = message_id,
                  .sender_id = row["sender_id"].as<int>(),
                  .sender_name = row["sender_name"].as<std::string>(),
                  .content = row["content"].as<std::string>(),
                  .message_type = row["message_type"].as<std::string>(),
                  .is_read = row["is_read"].as<bool>(),
                  .created_at = row["created_at"].as<std::string>(),
                  .metadata = row["metadata"].as<std::string>(),
                  /*.media = media_attachments.value_or({})});*/
                  .media = media_attachments.value_or(std::vector<MediaQuickInfo>())});    // 构建消息对象
    }
    auto resp = HttpResponse::newHttpResponse(k200OK, CT_APPLICATION_JSON);    // 返回200成功
    resp->setBody(glz::write_json(messages_list).value_or(""));    // 设置响应体
    callback(resp);
  } catch (const DrogonDbException& e) {    // 数据库异常处理
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError ret{.error = "Database error"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);    // 返回500错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 发送消息
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @param conversation_id 对话ID
 * @details 在指定对话中发送消息，支持文本和媒体类型，验证用户权限并发送实时通知。
 */
Task<> Chats::send_message(HttpRequestPtr req,
                           std::function<void(const HttpResponsePtr&)> callback,
                           std::string conversation_id) {
  std::string user_id =
      req->getAttributes()->get<std::string>("current_user_id");    // 获取当前用户ID

  auto conv_id_optional = convert::string_to_int(conversation_id);    // 转换对话ID
  if (!conv_id_optional || conv_id_optional.value() < 0) {    // 验证对话ID有效性
    SimpleError ret{.error = "Invalid conversation_id"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);    // 返回400错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
    co_return;
  }
  int conv_id = conv_id_optional.value();    // 获取对话ID

  SendMessageRequest send_msg_req;
  auto parse_error = utilities::strict_read_json(send_msg_req, req->getBody());    // 解析JSON请求体

  // Validation: content and media can't be both missing. If content is provided
  // (and media isn't), it can't be empty. If media is provided it should be
  // either null or an array.
  // 验证：内容和媒体不能同时缺失。如果提供了内容（且没有媒体），内容不能为空。如果提供了媒体，应该是null或数组。
  if (parse_error || (!send_msg_req.content && !send_msg_req.media) ||
      (send_msg_req.content && !send_msg_req.media &&
       send_msg_req.content->empty()) ||
      (send_msg_req.media && !send_msg_req.media->empty() &&
       send_msg_req.media->size() > service::MAX_MEDIA_SIZE)) {    // 验证请求参数
    LOG_INFO << "Message content or media is required, content a string, media "
                "an array of strings. Max media size is "
             << service::MAX_MEDIA_SIZE;
    SimpleError ret{.error =
                        "Message content or media is required, content a "
                        "string, media an array of strings"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);    // 返回400错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
    co_return;
  }

  std::string content = send_msg_req.content.value_or("");    // 获取消息内容

  std::string message_type = "text";
  if (send_msg_req.media.has_value()) {
    message_type = content.empty() ? "media" : "mixed";    // 确定消息类型
  }

  auto db = app().getDbClient();    // 获取数据库客户端

  try {
    int current_user_id = convert::string_to_int(user_id).value();    // 转换用户ID
    // 验证用户是否为对话参与者
    auto result = co_await db->execSqlCoro(
        "SELECT 1 FROM conversation_participants WHERE conversation_id = $1 "
        "AND "
        "user_id = $2",
        conv_id, current_user_id);    // 执行权限验证

    if (result.empty()) {    // 用户不是对话参与者
      SimpleError ret{.error = "Unauthorized access to conversation"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);    // 返回403错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }

    auto transaction = co_await db->newTransactionCoro();    // 创建数据库事务
    try {
      // 插入消息记录
      auto insert_result = co_await transaction->execSqlCoro(
          "INSERT INTO messages (conversation_id, sender_id, content, "
          "message_type) "
          "VALUES ($1, $2, $3, $4) RETURNING id, created_at",
          conv_id, current_user_id, content, message_type);    // 执行消息插入

      if (insert_result.empty()) {    // 插入失败
        throw std::runtime_error("Initial message insert failed");
      }

      int message_id = insert_result[0]["id"].as<int>();    // 获取消息ID
      std::string created_at = insert_result[0]["created_at"].as<std::string>();    // 获取创建时间

      std::expected<std::vector<MediaQuickInfo>, std::string> processed_media;
      if (send_msg_req.media.has_value() && !send_msg_req.media->empty()) {    // 处理媒体附件
        std::size_t media_array_size = send_msg_req.media->size();
        processed_media = co_await process_media_attachments(
            std::move(*send_msg_req.media), transaction, current_user_id,
            "message", message_id);    // 处理媒体附件
        if (!processed_media.has_value() ||
            processed_media->size() < media_array_size) {    // 媒体处理失败
          LOG_ERROR << " Some Media info was not found";
          transaction->rollback();    // 回滚事务
          std::string error_string;
          for (const auto& media_item : *processed_media) {
            error_string += media_item.filename + ", ";
          }
          SimpleError ret{.error =
                              std::format("Media info not found or processed, "
                                          "only the following media items "
                                          "were processed:\n{}",
                                          error_string)};
          auto resp = HttpResponse::newHttpResponse(k400BadRequest,
                                                    CT_APPLICATION_JSON);    // 返回400错误
          resp->setBody(glz::write_json(ret).value_or(""));
          callback(resp);
          co_return;
        }
      }

      std::string chat_topic = create_topic("chat", conversation_id);    // 创建通知主题

      NotificationMessage msg{
          .type = "message_sent",
          .id = conversation_id,
          .message = message_type != "text"
                         ? std::format("Media shared: {}", content)
                         : content,
          .modified_at = created_at};    // 构建通知消息

      ServiceManager::get_instance().get_publisher().publish(
          chat_topic, glz::write_json(msg).value_or(""));    // 发布通知
      SendMessageResponse ret{
          .status = "success",
          .message_id = message_id,
          .created_at = created_at,
          .message_type = message_type,
          .media = processed_media->empty()
                       ? std::nullopt
                       : std::make_optional(*processed_media)};    // 构建响应
      auto resp = HttpResponse::newHttpResponse(k200OK, CT_APPLICATION_JSON);    // 返回200成功
      resp->setBody(glz::write_json(ret).value_or(""));    // 设置响应体
      callback(resp);

      co_return;
    } catch (const std::exception& e) {    // 事务执行失败
      LOG_ERROR << "Failed to send message: " << e.what();
      transaction->rollback();    // 回滚事务
      SimpleError ret{.error =
                          std::format("Failed to send message: {}", e.what())};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);    // 返回500错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }
  } catch (const DrogonDbException& e) {    // 数据库异常处理
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError ret{.error = "Database error"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);    // 返回500错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 通过报价ID获取对话
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @param offer_id 报价ID
 * @details 根据报价ID获取相关对话，如果不存在则创建新对话，验证用户权限。
 */
Task<> Chats::get_conversation_by_offer(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
    std::string offer_id) {
  std::string current_user_id =
      req->getAttributes()->get<std::string>("current_user_id");    // 获取当前用户ID
  auto db = app().getDbClient();    // 获取数据库客户端

  auto offer_id_optional = convert::string_to_int(offer_id);    // 转换报价ID
  if (!offer_id_optional || offer_id_optional.value() < 0) {    // 验证报价ID有效性
    SimpleError ret{.error = "Invalid offer ID"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);    // 返回400错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
    co_return;
  }
  int offer_id_int = offer_id_optional.value();    // 获取报价ID

  try {
    // 查询报价和帖子信息
    auto result = co_await db->execSqlCoro(
        "SELECT o.*, p.user_id AS post_user_id "
        "FROM offers o "
        "JOIN posts p ON o.post_id = p.id "
        "WHERE o.id = $1",
        offer_id_int);    // 执行报价查询

    if (result.empty()) {    // 报价不存在
      SimpleError ret{.error = "Offer not found"};
      auto resp =
          HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);    // 返回404错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }

    int offer_user_id = result[0]["user_id"].as<int>();    // 获取报价用户ID
    int post_user_id = result[0]["post_user_id"].as<int>();    // 获取帖子用户ID

    // 验证用户权限：必须是报价用户或帖子用户
    if (convert::string_to_int(current_user_id).value() != offer_user_id &&
        convert::string_to_int(current_user_id).value() != post_user_id) {
      SimpleError ret{.error = "Unauthorized"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);    // 返回403错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }

    // 查询是否已存在对话
    auto conv_result = co_await db->execSqlCoro(
        "SELECT c.id FROM conversations c "
        "JOIN conversation_participants cp1 ON c.id = cp1.conversation_id "
        "JOIN conversation_participants cp2 ON c.id = cp2.conversation_id "
        "WHERE cp1.user_id = $1 AND cp2.user_id = $2 "
        "LIMIT 1",
        convert::string_to_int(current_user_id).value(),
        (convert::string_to_int(current_user_id).value() == offer_user_id
             ? post_user_id
             : offer_user_id));    // 执行对话查询

    if (conv_result.empty()) {    // 对话不存在，创建新对话
      auto new_conv_result = co_await db->execSqlCoro(
          "INSERT INTO conversations (name) VALUES ($1) RETURNING id",
          "Offer #" + offer_id + " Conversation");    // 创建对话

      if (new_conv_result.empty()) {    // 创建失败
        SimpleError ret{.error = "Failed to create conversation"};
        auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                  CT_APPLICATION_JSON);    // 返回500错误
        resp->setBody(glz::write_json(ret).value_or(""));
        callback(resp);
        co_return;
      }

      int conversation_id = new_conv_result[0]["id"].as<int>();    // 获取新对话ID

      try {
        // 添加对话参与者
        co_await db->execSqlCoro(
            "INSERT INTO conversation_participants "
            "(conversation_id, user_id) VALUES ($1, $2), ($1, $3)",
            conversation_id, convert::string_to_int(current_user_id).value(),
            (convert::string_to_int(current_user_id).value() == offer_user_id
                 ? post_user_id
                 : offer_user_id));    // 插入参与者

        GetConversationByOfferResponse response{
            .status = "success",
            .conversation_id = conversation_id,
            .is_new = true};    // 构建响应

        auto resp = HttpResponse::newHttpResponse(k200OK, CT_APPLICATION_JSON);    // 返回200成功
        resp->setBody(glz::write_json(response).value_or(""));    // 设置响应体
        callback(resp);
      } catch (const DrogonDbException& e) {    // 添加参与者失败
        LOG_ERROR << "Database error adding participants: " << e.base().what();
        SimpleError ret{.error = "Database error"};
        auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                  CT_APPLICATION_JSON);    // 返回500错误
        resp->setBody(glz::write_json(ret).value_or(""));
        callback(resp);
      }
    } else {    // 对话已存在
      GetConversationByOfferResponse response{
          .status = "success",
          .conversation_id = conv_result[0]["id"].as<int>(),
          .is_new = false};    // 构建响应

      auto resp = HttpResponse::newHttpResponse(k200OK, CT_APPLICATION_JSON);    // 返回200成功
      resp->setBody(glz::write_json(response).value_or(""));    // 设置响应体
      callback(resp);
    }
  } catch (const DrogonDbException& e) {    // 数据库异常处理
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError ret{.error = "Database error"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);    // 返回500错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 标记消息为已读
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @param conversation_id 对话ID
 * @details 将指定对话中当前用户未读的消息标记为已读，验证用户权限。
 */
Task<> Chats::mark_messages_as_read(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
    std::string conversation_id) {
  std::string user_id =
      req->getAttributes()->get<std::string>("current_user_id");    // 获取当前用户ID

  auto conv_id_optional = convert::string_to_int(conversation_id);    // 转换对话ID
  if (!conv_id_optional || conv_id_optional.value() < 0) {    // 验证对话ID有效性
    SimpleError ret{.error = "Invalid conversation_id"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);    // 返回400错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
    co_return;
  }
  int conv_id = conv_id_optional.value();    // 获取对话ID

  auto db = app().getDbClient();    // 获取数据库客户端

  try {
    // 验证用户是否为对话参与者
    auto result = co_await db->execSqlCoro(
        "SELECT 1 FROM conversation_participants WHERE conversation_id = $1 "
        "AND "
        "user_id = $2",
        conv_id, convert::string_to_int(user_id).value());    // 执行权限验证

    if (result.empty()) {    // 用户不是对话参与者
      SimpleError ret{.error = "Unauthorized access to conversation"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);    // 返回403错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }

    // 更新消息阅读状态
    auto update_result = co_await db->execSqlCoro(
        "UPDATE messages SET is_read = true "
        "WHERE conversation_id = $1 AND sender_id != $2 AND is_read = false "
        "RETURNING id",
        conv_id, convert::string_to_int(user_id).value());    // 更新未读消息为已读

    MarkMessagesAsReadResponse ret{
        .status = "success", .messages_marked = (int)update_result.size()};    // 构建响应
    auto resp = HttpResponse::newHttpResponse(k200OK, CT_APPLICATION_JSON);    // 返回200成功
    resp->setBody(glz::write_json(ret).value_or(""));    // 设置响应体
    callback(resp);
  } catch (const DrogonDbException& e) {    // 数据库异常处理
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError ret{.error = "Database error"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);    // 返回500错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 获取未读消息数量
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @details 查询当前用户所有对话中的未读消息总数。
 */
Task<> Chats::get_unread_count(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback) {
  std::string user_id =
      req->getAttributes()->get<std::string>("current_user_id");    // 获取当前用户ID

  auto db = app().getDbClient();    // 获取数据库客户端

  try {
    // 查询未读消息数量
    auto result = co_await db->execSqlCoro(
        "SELECT COUNT(*) as unread_count "
        "FROM messages m "
        "JOIN conversation_participants cp ON m.conversation_id = "
        "cp.conversation_id "
        "WHERE cp.user_id = $1 AND m.sender_id != $1 AND m.is_read = false",
        convert::string_to_int(user_id).value());    // 执行未读消息计数查询

    UnreadCountResponse ret{.unread_count =
                                result[0]["unread_count"].as<int>()};    // 构建响应
    auto resp = HttpResponse::newHttpResponse(k200OK, CT_APPLICATION_JSON);    // 返回200成功
    resp->setBody(glz::write_json(ret).value_or(""));    // 设置响应体
    callback(resp);
  } catch (const DrogonDbException& e) {    // 数据库异常处理
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError ret{.error = "Database error"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);    // 返回500错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
  }

  co_return;
}
