/**
 * @file redis_pub_manager.hpp
 * @brief Redis发布管理器头文件
 * @details 基于Redis实现的发布管理器，用于通过Redis的发布/订阅功能发送消息。
 */

#ifndef REDIS_PUB_MANAGER_HPP
#define REDIS_PUB_MANAGER_HPP

#include <sw/redis++/redis++.h>       // 引入Redis++客户端头文件

#include <string>                     // 引入字符串类头文件

/**
 * @class PubManager
 * @brief Redis发布管理器类
 * @details 使用Redis++客户端实现消息发布功能，支持跨进程通信。
 */
class PubManager {
 public:
  /**
   * @brief 默认构造函数（禁用）
   */
  PubManager() = delete;
  /**
   * @brief 拷贝构造函数（禁用）
   */
  PubManager(const PubManager&) = delete;
  /**
   * @brief 赋值运算符（禁用）
   */
  PubManager& operator=(const PubManager&) = delete;

  /**
   * @brief 构造函数（使用连接选项）
   * @param conn_opts Redis连接选项
   * @details 使用指定的连接选项创建Redis客户端。
   */
  PubManager(const sw::redis::ConnectionOptions& conn_opts)
      : redis_(std::make_unique<sw::redis::Redis>(conn_opts)) {}

  /**
   * @brief 构造函数（使用URI）
   * @param uri Redis连接URI
   * @details 使用指定的URI创建Redis客户端。
   */
  PubManager(const std::string& uri)
      : redis_(std::make_unique<sw::redis::Redis>(uri)) {}

  /**
   * @brief 获取Redis订阅者
   * @return Redis订阅者智能指针
   * @details 创建并返回一个新的Redis订阅者实例。
   */
  std::unique_ptr<sw::redis::Subscriber> get_redis_subscriber() const {
    return std::make_unique<sw::redis::Subscriber>(redis_->subscriber());
  }

  /**
   * @brief 发布消息
   * @param topic 消息主题
   * @param message 消息内容
   * @details 通过Redis发布指定主题的消息。
   */
  void publish(const std::string& topic, const std::string& message) {
    redis_->publish(topic, message);    // 使用Redis发布消息
  }

 private:
  std::unique_ptr<sw::redis::Redis> redis_;    // Redis客户端实例
};

#endif  // REDIS_PUB_MANAGER_HPP
