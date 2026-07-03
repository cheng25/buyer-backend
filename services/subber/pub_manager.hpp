/**
 * @file pub_manager.hpp
 * @brief ZeroMQ发布管理器头文件
 * @details 基于ZeroMQ实现的发布管理器，用于在进程内发布消息到订阅者。
 */

#ifndef PUB_MANAGER_HPP
#define PUB_MANAGER_HPP

#include <string>                   // 引入字符串类头文件
#include <zmq.hpp>                  // 引入ZeroMQ头文件

/**
 * @class PubManager
 * @brief ZeroMQ发布管理器类
 * @details 使用ZeroMQ的PUB套接字实现消息发布功能，支持进程内通信。
 */
class PubManager {
 public:
  /**
   * @brief 默认构造函数（禁用）
   */
  PubManager() = delete;
  // Move only 仅支持移动
  /**
   * @brief 拷贝构造函数（禁用）
   */
  PubManager(const PubManager &) = delete;
  /**
   * @brief 赋值运算符（禁用）
   */
  PubManager &operator=(const PubManager &) = delete;

  /**
   * @brief 构造函数
   * @param context ZeroMQ上下文
   * @details 创建PUB类型的ZeroMQ套接字并绑定到inproc://pubsub地址。
   */
  PubManager(zmq::context_t &context)
      : socket_(context, zmq::socket_type::pub) {
    socket_.bind("inproc://pubsub");    // 绑定到进程内通信地址
  }

  /**
   * @brief 发布消息
   * @param topic 消息主题
   * @param message 消息内容
   * @details 使用ZeroMQ多部分消息发送，先发送主题再发送消息内容。
   */
  void publish(const std::string &topic, const std::string &message) {
    zmq::message_t topic_msg(topic);    // 创建主题消息
    zmq::message_t data_msg(message);   // 创建数据消息
    socket_.send(topic_msg, zmq::send_flags::sndmore);    // 发送主题，标记还有更多消息
    socket_.send(data_msg, zmq::send_flags::none);        // 发送数据，结束消息发送
  }

 private:
  zmq::socket_t socket_;    // ZeroMQ PUB套接字
};

#endif  // PUB_MANAGER_HPP
