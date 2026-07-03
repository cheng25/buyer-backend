/**
 * @file notification_websocket.cpp
 * @brief 通知WebSocket控制器实现文件
 * @details 实现WebSocket连接管理、消息处理和通知推送功能，支持用户订阅标签和位置等实体。
 */
#include "notification_websocket.hpp"

#include "../services/service_manager.hpp"
#include "../utilities/json_manipulation.hpp"
#include "common_req_n_resp.hpp"

using drogon::app;
using drogon::HttpRequestPtr;
using drogon::WebSocketConnectionPtr;
using drogon::WebSocketMessageType;

/**
 * @struct WelcomeMessage
 * @brief WebSocket欢迎消息数据结构
 * @details 客户端连接成功后发送的欢迎消息。
 */
struct WelcomeMessage {
  std::string type;              // 消息类型
  std::string message;           // 消息内容
};

/**
 * @brief 处理从客户端接收到的新消息
 * @details 根据消息类型进行相应处理，支持文本消息、二进制消息、Ping/Pong心跳和连接关闭消息。
 * @param wsConnPtr WebSocket连接指针
 * @param message 从客户端接收到的消息
 * @param type 消息类型
 */
void NotificationWebSocket::handleNewMessage(
    const WebSocketConnectionPtr& wsConnPtr, std::string&& message,
    const WebSocketMessageType& type) {
  auto connId = wsConnPtr->getContext<std::string>();

  switch (type) {
    case WebSocketMessageType::Text:
      if (message.empty()) {
        LOG_WARN << "Received empty message from user " << *connId;
        SimpleError error{.error = "Received empty message"};
        wsConnPtr->send(glz::write_json(error).value_or(""));
      }
      // else if (message_needs_parsing) {
      //   // For incoming messages, if they need to be parsed, define a struct
      //   // like `IncomingWebSocketMessage` and use
      //   `utilities::strict_read_json`
      //   // Example:
      //   // IncomingWebSocketMessage incoming_msg;
      //   // auto parse_error = utilities::strict_read_json(incoming_msg,
      //   message);
      //   // if (parse_error) {
      //   //   SimpleError error{.error = "Invalid message format"};
      //   //   wsConnPtr->send(glz::write_json(error).value_or(""));
      //   //   return;
      //   // }
      //   // Process incoming_msg...
      // }
      break;
    case WebSocketMessageType::Binary:
      break;
    case WebSocketMessageType::Ping:
      wsConnPtr->send("", drogon::WebSocketMessageType::Pong); // Send Pong to client
      break;
    case WebSocketMessageType::Pong:
      wsConnPtr->send("", drogon::WebSocketMessageType::Ping);// Send Ping to client
      break;
    case WebSocketMessageType::Close:
      wsConnPtr->forceClose();// Force Close the connection 强制中断连接
      return;
    default:
      LOG_WARN << "Received unknown message type";
  }
}

/**
 * @brief 处理客户端新连接
 * @details 验证用户身份，注册WebSocket连接到连接管理器，并订阅用户已有的标签订阅。
 * @param req HTTP请求指针
 * @param wsConnPtr WebSocket连接指针
 */
void NotificationWebSocket::handleNewConnection(
    const HttpRequestPtr& req, const WebSocketConnectionPtr& wsConnPtr) {
  std::string current_user_id =
      req->getAttributes()->get<std::string>("current_user_id");

  if (current_user_id.empty()) {
    LOG_ERROR << "No authenticated/registered user for WebSocket connection";
    wsConnPtr->forceClose();// Force Close the connection 强制中断连接
    return;
  }

  std::shared_ptr<std::string> ptr =
      std::make_shared<std::string>(current_user_id);
  wsConnPtr->setContext(std::static_pointer_cast<void>(ptr));

  // Register WebSocket connection with Connection manager 添加WebSocket连接到连接管理器中
  ServiceManager::get_instance().get_connection_manager().add_connection(
      current_user_id, wsConnPtr);

  // Subscribe user to their existing tag subscriptions 订阅用户到他们的现有标签订阅
  try {
    subscribe_user_to_existing_subs(current_user_id);
  } catch (const std::exception& e) {
    LOG_INFO << "\n\n\nException in subscribing user to existing tags: "
             << e.what();
  }

  LOG_INFO << "WebSocket connected for user: " << current_user_id;

  WelcomeMessage welcome{.type = "connected",
                         .message = "Connected to notification service"};
  wsConnPtr->send(glz::write_json(welcome).value_or(""));
}

/**
 * @brief 处理连接关闭
 * @details 从连接管理器中移除WebSocket连接，并取消用户的所有订阅。
 * @param wsConnPtr WebSocket连接指针
 */
void NotificationWebSocket::handleConnectionClosed(
    const WebSocketConnectionPtr& wsConnPtr) {
  auto connId = wsConnPtr->getContext<std::string>();
  // Remove WebSocket connection from Connection manager 删除WebSocket连接从连接管理器中
  ServiceManager::get_instance().get_connection_manager().remove_connection(
      *connId, wsConnPtr);
  // Unsubscribe user from all subscriptions 取消用户对所有订阅的订阅
  ServiceManager::get_instance().get_connection_manager().unsubscribe(*connId);
  LOG_INFO << "WebSocket disconnected for user: " << *connId;
}

/**
 * @brief 为用户订阅现有订阅
 * @details 从数据库查询用户已保存的订阅列表，并为用户订阅相应的频道。
 * @param user_id 用户ID
 */
void NotificationWebSocket::subscribe_user_to_existing_subs(
    std::string user_id) {
  try {
    auto db = app().getDbClient();
    db->execSqlAsync(
        "SELECT subscription FROM user_subscriptions WHERE user_id = $1",
        [user_id](const drogon::orm::Result& result) {
          for (const auto& row : result) {
            std::string channel = row["subscription"].as<std::string>();
            try {
              // Subscribe to tag 订阅标签
              ServiceManager::get_instance().get_subscriber().subscribe(
                  channel);
              // Add subscription to connection manager 添加订阅到连接管理器中
              ServiceManager::get_instance().get_connection_manager().subscribe(
                  channel, user_id);
            } catch (const std::exception& e) {
              LOG_ERROR << "Failed to subscribe user to existing tags: "
                        << e.what();
            }
          }
        },
        [=](const drogon::orm::DrogonDbException& e) {
          LOG_ERROR << "Failed to get user tag subscriptions: "
                    << e.base().what() << "Subscription failed";
        },
        user_id);
    LOG_INFO << "user_id " << user_id << " subscribed to existing tags: ";
  }

  catch (...) {
    LOG_ERROR << "User Notification subscriptions failed";
  }
}
