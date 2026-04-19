#ifndef NOTIFICATION_WEBSOCKET_HPP
#define NOTIFICATION_WEBSOCKET_HPP

#include <drogon/WebSocketController.h>
/**
 * @brief NotificationWebSocket class for handling WebSocket connections for
 * notifications
 * NotificationWebSocket类用于处理WebSocket连接，用于处理通知
 */
class NotificationWebSocket
    : public drogon::WebSocketController<NotificationWebSocket> {
 public:
  /**
   * @brief Handle new message received from client 处理从客户端接收到的新消息
   * @param wsConnPtr WebSocket connection pointer WebSocket连接指针
   * @param message Message received from client 从客户端接收到的消息
   * @param type Type of message received 消息类型
   */
  void handleNewMessage(const drogon::WebSocketConnectionPtr& wsConnPtr,
                        std::string&& message,
                        const drogon::WebSocketMessageType& type) override;

  /**
   * @brief Handle new connection from client 处理从客户端接新的连接
   * @param req Http request pointer Http请求指针
   * @param wsConnPtr WebSocket connection pointer WebSocket连接指针
   */
  void handleNewConnection(
      const drogon::HttpRequestPtr& req,
      const drogon::WebSocketConnectionPtr& wsConnPtr) override;

  /**
   * @brief Handle connection closed 处理连接关闭
   * @param wsConnPtr WebSocket connection pointer WebSocket连接指针
   */
  void handleConnectionClosed(
      const drogon::WebSocketConnectionPtr& wsConnPtr) override;

  WS_PATH_LIST_BEGIN
  WS_PATH_ADD("/ws/notifications", "WebSocketAuthMiddleware");
  WS_PATH_LIST_END

 private:
  /**
   * @brief Subscribe user to existing subscriptions 为用户订阅现有订阅
   * @param user_id User ID 用户ID
   */
  static void subscribe_user_to_existing_subs(std::string user_id);
};

#endif  // NOTIFICATION_WEBSOCKET_HPP
