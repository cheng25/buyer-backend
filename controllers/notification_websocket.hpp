/**
 * @file notification_websocket.hpp
 * @brief 通知WebSocket控制器头文件
 * @details 定义NotificationWebSocket类，处理WebSocket连接管理、消息处理和通知推送。
 */
#ifndef NOTIFICATION_WEBSOCKET_HPP
#define NOTIFICATION_WEBSOCKET_HPP

#include <drogon/WebSocketController.h>

/**
 * @class NotificationWebSocket
 * @brief 通知WebSocket控制器类
 * @details 继承自drogon::WebSocketController，处理WebSocket连接管理、消息处理和通知推送功能。
 */
class NotificationWebSocket
    : public drogon::WebSocketController<NotificationWebSocket> {
 public:
  /**
   * @brief 处理从客户端接收到的新消息
   * @details 根据消息类型进行相应处理，支持文本消息、二进制消息、Ping/Pong心跳和连接关闭消息。
   * @param wsConnPtr WebSocket连接指针
   * @param message 从客户端接收到的消息
   * @param type 消息类型
   */
  void handleNewMessage(const drogon::WebSocketConnectionPtr& wsConnPtr,
                        std::string&& message,
                        const drogon::WebSocketMessageType& type) override;

  /**
   * @brief 处理客户端新连接
   * @details 验证用户身份，注册WebSocket连接到连接管理器，并订阅用户已有的标签订阅。
   * @param req HTTP请求指针
   * @param wsConnPtr WebSocket连接指针
   */
  void handleNewConnection(
      const drogon::HttpRequestPtr& req,
      const drogon::WebSocketConnectionPtr& wsConnPtr) override;

  /**
   * @brief 处理连接关闭
   * @details 从连接管理器中移除WebSocket连接，并取消用户的所有订阅。
   * @param wsConnPtr WebSocket连接指针
   */
  void handleConnectionClosed(
      const drogon::WebSocketConnectionPtr& wsConnPtr) override;

  /**
   * @brief WebSocket路由列表开始宏
   * @details 注册WebSocket连接路径和认证中间件。
   */
  WS_PATH_LIST_BEGIN
  /**
   * @brief 注册通知WebSocket连接路径
   * @details 路径为/ws/notifications，需要通过WebSocketAuthMiddleware认证。
   */
  WS_PATH_ADD("/ws/notifications", "WebSocketAuthMiddleware");
  /**
   * @brief WebSocket路由列表结束宏
   * @details 标记路由注册结束。
   */
  WS_PATH_LIST_END

 private:
  /**
   * @brief 为用户订阅现有订阅
   * @details 从数据库查询用户已保存的订阅列表，并为用户订阅相应的频道。
   * @param user_id 用户ID
   */
  static void subscribe_user_to_existing_subs(std::string user_id);
};

#endif  // NOTIFICATION_WEBSOCKET_HPP
