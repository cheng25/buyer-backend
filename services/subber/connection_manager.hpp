/**
 * @file connection_manager.hpp
 * @brief WebSocket连接管理器头文件
 * @details 提供线程安全的WebSocket连接管理、订阅管理和消息广播功能。
 */

#ifndef CONNECTION_MANAGER_HPP
#define CONNECTION_MANAGER_HPP

#include <ankerl/unordered_dense.h>              // 引入高性能哈希容器库
#include <drogon/WebSocketConnection.h>         // 引入Drogon WebSocket连接头文件
#include <drogon/drogon.h>                      // 引入Drogon框架核心头文件

#include <format>                               // 引入格式化输出头文件
#include <mutex>                                // 引入互斥锁头文件
#include <string>                               // 引入字符串类头文件
#include <unordered_map>                        // 引入无序映射容器头文件
#include <unordered_set>                        // 引入无序集合容器头文件

#include "../../controllers/common_req_n_resp.hpp"    // 引入通用请求响应结构
#include "../../utilities/json_manipulation.hpp"       // 引入JSON操作工具

// enum class notification_type : uint8_t {}

// notification types 通知类型
// post_created 帖子创建
// post_updated 帖子更新

// offer_created 报价创建
// offer_updated 报价更新
// offer_negotiated 报价协商
// offer_accepted 报价接受
// offer_rejected 报价拒绝

// chat_created 聊天创建
// message_sent 消息发送

/**
 * @brief 创建主题字符串
 * @param topic_type 主题类型
 * @param topic_id 主题ID
 * @return 格式化的主题字符串
 * @details 将主题类型和主题ID组合成格式为"type:id"的主题字符串。
 */
inline std::string create_topic(const std::string &topic_type,
                                const std::string &topic_id) {
  return std::format("{}:{}", topic_type, topic_id);
}

// inline drogon::Task<> remove_user_subscription(std::string user_id, const
// std::string &topic) {

/**
 * @brief 移除用户订阅
 * @param user_id 用户ID
 * @param topic 订阅主题
 * @details 从数据库中移除用户对指定主题的订阅。
 */
inline void remove_user_subscription(std::string user_id,
                                     const std::string &topic) {
  try {
    auto db = drogon::app().getDbClient();    // 获取数据库客户端

    /*     co_await db->execSqlCoro(
            "DELETE FROM user_subscriptions "
            "WHERE user_id = $1 AND subscription = $2 "
            "RETURNING id",
            std::stoi(user_id), topic); */

    // 异步执行SQL删除订阅记录
    db->execSqlAsync(
        "DELETE FROM user_subscriptions "
        "WHERE user_id = $1 AND subscription = $2 "
        "RETURNING id",
        [](const drogon::orm::Result &) {
          LOG_INFO << "Subscription removed from database";
        },
        [](const drogon::orm::DrogonDbException &e) {
          LOG_ERROR << "Failed to remove subscription: " << e.base().what();
        },
        user_id, topic);
  } catch (...) {
    LOG_ERROR << "Failed to remove subscription\n";
  }
}

// 存储用户订阅

/**
 * @brief 存储用户订阅
 * @param user_id 用户ID
 * @param topic 订阅主题
 * @details 将用户订阅信息存储到数据库中，使用UPSERT操作避免重复插入。
 */
inline void store_user_subscription(const std::string &user_id,
                                    const std::string &topic) {
  try {
    const auto db = drogon::app().getDbClient();    // 获取数据库客户端
    // 异步执行 SQL//存在冲突不采取任何行动
    db->execSqlAsync(
        "INSERT INTO user_subscriptions (user_id, subscription) "
        "VALUES ($1, $2) "
        "ON CONFLICT (user_id, subscription) DO NOTHING "
        "RETURNING id",
        [](const drogon::orm::Result &) {
          LOG_INFO << "Subscription stored in database";
        },
        [](const drogon::orm::DrogonDbException &e) {
          LOG_ERROR << "Failed to store subscription: " << e.base().what();
        },
        user_id, topic);
  } catch (...) {
    LOG_ERROR << "Failed to store subscription\n";
  }
}

/**
 * @brief 将通知存储到数据库
 * @param user_id 用户ID
 * @param message 通知消息（JSON格式）
 * @details 解析通知消息并将其存储到数据库中。
 */
inline void store_notification_in_db(const std::string &user_id,
                                     const std::string &message) {
  NotificationMessage notification;
  auto parse_error = utilities::strict_read_json(notification, message);    // 解析JSON消息

  if (parse_error) {
    LOG_ERROR << "Failed to parse message as NotificationMessage using glaze";
    return;
  }

  try {
    auto db = drogon::app().getDbClient();    // 获取数据库客户端
    // 异步执行SQL插入通知记录
    db->execSqlAsync(
        "INSERT INTO notifications (user_id, type, message) VALUES ($1, $2, "
        "$3)",
        [](const drogon::orm::Result &) {
          LOG_INFO << "Notification stored in database";
        },
        [](const drogon::orm::DrogonDbException &e) {
          LOG_ERROR << "Failed to store notification: " << e.base().what();
        },
        user_id, notification.type, notification.message);
  } catch (...) {
    LOG_ERROR << "Failed to store notification\n";
  }
}

/**
 * @brief Manages WebSocket connections, subscriptions, and message broadcasting
 *        管理WebSocket连接、订阅和消息广播
 *
 * This class provides thread-safe methods for:
 * - Adding and removing WebSocket connections 添加和移除WebSocket连接
 * - Subscribing and unsubscribing connections to topics 订阅和取消订阅主题
 * - Broadcasting messages to subscribed connections 向订阅的连接广播消息
 *
 * Uses thread-safe mechanisms like mutex locks to prevent race conditions
 * when manipulating connection and subscriber data structures.
 * 使用互斥锁等线程安全机制防止在操作连接和订阅者数据结构时发生竞态条件。
 *
 * Current Design: Multiple connections per user
 * 当前设计：每个用户多个连接
 * Improvements: Create limit for number of concurrent connection or switch to
 * Alternative design
 * 改进方向：限制并发连接数或切换到替代设计
 *
 * Alternative Design:
 * 替代设计：
 * One connection per user, relegation older connections:
 * 每个用户一个连接，淘汰旧连接：
 * To support this in the current code, always broadcast from the front.
 * 要在当前代码中支持此设计，始终从前端广播。
 * Then on removal of the main connection,
 * 然后在主连接移除时，
 * the second newest connection automatically becomes main connection.
 * 第二个最新连接自动成为主连接。
 */
class ConnectionManager {
 public:
  /**
   * @brief 添加WebSocket连接
   * @param conn_id 连接ID（用户ID）
   * @param conn WebSocket连接指针
   * @details 将WebSocket连接添加到指定用户的连接列表中，新连接成为主连接。
   */
  void add_connection(const std::string conn_id,
                      const drogon::WebSocketConnectionPtr &conn) {
    std::lock_guard<std::mutex> lock(mutex_);    // 获取互斥锁

    // multiple connections per user 每个用户多个连接
    connections_[conn_id].emplace_front(conn);  // make it the main connection 使其成为主连接
  }

  /**
   * @brief 移除WebSocket连接
   * @param conn_id 连接ID（用户ID）
   * @param conn WebSocket连接指针
   * @details 从指定用户的连接列表中移除指定的WebSocket连接。
   */
  void remove_connection(const std::string conn_id,
                         const drogon::WebSocketConnectionPtr &conn) {
    std::lock_guard<std::mutex> lock(mutex_);    // 获取互斥锁

    auto it = connections_.find(conn_id);    // 查找用户连接列表
    if (it != connections_.end() && !it->second.empty()) {
      //connections_[conn_id].remove(conn); // 第2次查找（冗余！）
      it->second.remove(conn); // 直接使用已找到的迭代器
    }
  }

  /**
   * @brief 订阅主题
   * @param topic 主题名称
   * @param conn_id 连接ID（用户ID）
   * @details 将用户订阅到指定主题，线程安全。
   */
  void subscribe(const std::string &topic, const std::string conn_id) {
    try {
      std::lock_guard<std::mutex> lock(mutex_);    // 获取互斥锁
      subscribers_[topic].insert(conn_id);    // 将用户添加到主题订阅者列表
    } catch (const std::system_error &e) {
      LOG_ERROR << "Lock acquisition failed, topic subscription failed: "
                << e.what();
    } catch (const std::bad_alloc &e) {
      LOG_ERROR << "Out of Memory, no further subscriptions: " << e.what();
    } catch (const std::exception &e) {
      LOG_ERROR << "Topic Subscription failed: " << e.what();
    }

    // // search before insertion to prevent crashes.
    // // For debugging - can be removed in prod
    // LOG_INFO << "Content of topic " << topic << " , Connection ID: " <<
    // conn_id
    //          << "\n";
    // auto it = connections_.find(conn_id);
    // if (it != connections_.end()) {
    //   // send to only the main (newest) connection
    //   it->second.front()->send("Subscribed to topic: " + topic);
    // }
  }

  /**
   * @brief 取消订阅所有主题
   * @param conn_id 连接ID（用户ID）
   * @details 从所有主题中取消指定用户的订阅。
   */
  void unsubscribe(const std::string &conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);    // 获取互斥锁
    for (auto &[topic, ids] : subscribers_) {
      if (connections_[conn_id].empty()) {
        ids.erase(conn_id);    // 从主题订阅者列表中移除用户
        connections_.erase(conn_id);    // 移除用户连接
      }
    }
  }

  /**
   * @brief 取消订阅指定主题
   * @param conn_id 连接ID（用户ID）
   * @param topic 主题名称
   * @details 从指定主题中取消用户的订阅。
   */
  void unsubscribe_user_from_topic(const std::string &conn_id,
                                   std::string &topic) {
    std::lock_guard<std::mutex> lock(mutex_);    // 获取互斥锁
    subscribers_[topic].erase(conn_id);    // 从主题订阅者列表中移除用户
  }

  /**
   * @brief 广播消息到主题订阅者
   * @param topic 主题名称
   * @param message 消息内容（JSON格式）
   * @details 将消息广播到所有订阅指定主题的用户，并将通知存储到数据库。
   */
  void broadcast(const std::string &topic, const std::string &message) {
    std::lock_guard<std::mutex> lock(mutex_);    // 获取互斥锁
    for (const auto &conn_id : subscribers_[topic]) {
      auto it = connections_.find(conn_id);    // 查找用户连接
      if (it != connections_.end()) {
        // store notification in DB 将通知存储到数据库
        store_notification_in_db(conn_id, message);
        // send notification 发送通知
        for (auto &conn : it->second) {
          conn->send(message);
        }
      }
    }
  }

 private:
  // <user_id, <connection>> 用户ID到连接列表的映射
  ankerl::unordered_dense::map<std::string,
                               std::list<drogon::WebSocketConnectionPtr>>
      connections_;

  //<topic,<user_id>> 主题到用户ID集合的映射
  ankerl::unordered_dense::map<std::string,
                               ankerl::unordered_dense::set<std::string>>
      subscribers_;
  std::mutex mutex_;    // 互斥锁，确保线程安全
};

#endif  // CONNECTION_MANAGER_HPP
