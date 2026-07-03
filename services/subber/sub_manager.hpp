/**
 * @file sub_manager.hpp
 * @brief ZeroMQ订阅管理器头文件
 * @details 基于ZeroMQ实现的订阅管理器，用于接收发布的消息并广播给WebSocket连接。
 */

#ifndef SUB_MANAGER_HPP
#define SUB_MANAGER_HPP

#include <stop_token>               // 引入停止令牌头文件
#include <thread>                   // 引入线程头文件
#include <zmq.hpp>                  // 引入ZeroMQ头文件

#include "connection_manager.hpp"   // 引入连接管理器头文件

/**
 * @class SubManager
 * @brief ZeroMQ订阅管理器类
 * @details 使用ZeroMQ的SUB套接字接收消息，并通过连接管理器广播给WebSocket客户端。
 */
class SubManager {
 public:
  /**
   * @brief 默认构造函数（禁用）
   */
  SubManager() = delete;
  // Move only 仅支持移动
  /**
   * @brief 拷贝构造函数（禁用）
   */
  SubManager(const SubManager &) = delete;
  /**
   * @brief 赋值运算符（禁用）
   */
  SubManager &operator=(const SubManager &) = delete;

  /**
   * @brief 构造函数
   * @param context ZeroMQ上下文
   * @param manager 连接管理器引用
   * @details 创建SUB类型的ZeroMQ套接字并连接到inproc://pubsub地址。
   */
  SubManager(zmq::context_t &context, ConnectionManager &manager)
      : socket_(context, zmq::socket_type::sub), conn_mgr_(manager) {
    socket_.connect("inproc://pubsub");    // 连接到进程内通信地址
  }

  /**
   * @brief 订阅主题
   * @param topic 主题名称
   * @details 设置ZeroMQ套接字订阅指定主题。
   */
  void subscribe(const std::string &topic) {
    socket_.set(zmq::sockopt::subscribe, topic);    // 设置订阅主题
  }

  /**
   * @brief 启动订阅线程
   * @details 创建后台线程，使用非阻塞方式接收ZeroMQ消息并广播给WebSocket客户端。
   */
  void run() {
    // // TODO: BENCHMARK which is best

    // sub_thread_ = std::jthread([this](std::stop_token stoken) {
    //     zmq::recv_result_t res;
    //     while (!stoken.stop_requested()) {
    //         zmq::message_t topic_msg;
    //         zmq::message_t data_msg;
    //         res = socket_.recv(topic_msg);
    //         if(!res.has_value()) {
    //             std::cout << "Error receiving message: " <<
    //             topic_msg.to_string() << std::endl;
    //         }
    //         res = socket_.recv(data_msg);
    //         if(!res.has_value()) {
    //             std::cout << "Error receiving message: " <<
    //             data_msg.to_string() << std::endl;
    //         }

    //         // std::string topic(static_cast<char*>(topic_msg.data()),
    //         topic_msg.size()); std::string topic = topic_msg.to_string();
    //         std::string message = data_msg.to_string();
    //         conn_mgr_.broadcast(topic, message);
    //     }
    // });

    // with a stop token, no blocking + timeout 使用停止令牌，非阻塞+超时
    sub_thread_ = std::jthread([this](std::stop_token stoken) {
      zmq::recv_result_t res;
      while (!stoken.stop_requested()) {    // 循环直到收到停止请求
        zmq::message_t topic_msg;    // 创建主题消息对象
        zmq::message_t data_msg;     // 创建数据消息对象
        res = socket_.recv(topic_msg, zmq::recv_flags::dontwait);    // 非阻塞接收主题
        if (!res.has_value()) {
          if (zmq_errno() == EAGAIN) {
            // No message available, wait for a short period before trying again
            // 没有消息可用，等待一段时间后重试
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
          } else {
            // Handle other errors 处理其他错误
            std::cerr << "Error receiving a message";
          }
        }
        res = socket_.recv(data_msg, zmq::recv_flags::dontwait);    // 非阻塞接收数据
        if (!res.has_value()) {
          if (zmq_errno() == EAGAIN) {
            // No message available, wait for a short period before trying again
            // 没有消息可用，等待一段时间后重试
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
          } else {
            // Handle other errors 处理其他错误
            std::cerr << "Error receiving a message";
          }
        }

        // std::string topic(static_cast<char*>(topic_msg.data()),
        // topic_msg.size());
        std::string topic = topic_msg.to_string();    // 将主题消息转换为字符串
        std::string message = data_msg.to_string();   // 将数据消息转换为字符串
        conn_mgr_.broadcast(topic, message);          // 广播消息给订阅该主题的客户端
      }
    });

    // polling implementation 轮询实现（注释掉）
    // sub_thread_ = std::jthread([this](std::stop_token stoken) {
    //     zmq::recv_result_t res;
    //     zmq::pollitem_t items[] = {{socket_, 0, ZMQ_POLLIN, 0}};
    //     zmq::message_t topic_msg;
    //     zmq::message_t data_msg;
    //     while (!stoken.stop_requested()) {
    //         zmq::poll(items, 1, 10); // 10ms timeout
    //         if (items[0].revents & ZMQ_POLLIN) {
    //             // Message available, receive it
    //             res = socket_.recv(topic_msg);
    //             if (res.has_value()) {
    //                 // Process the message
    //                 res = socket_.recv(data_msg);
    //                 if (res.has_value()) {
    //                     std::string topic = topic_msg.to_string();
    //                     std::string message = data_msg.to_string();
    //                     conn_mgr_.broadcast(topic, message);
    //                 }
    //             }
    //         }
    //     }
    // });
  }

  /**
   * @brief 停止订阅线程
   * @details 请求线程停止并等待线程结束。
   */
  void stop() {
    if (sub_thread_.joinable()) {
      sub_thread_.request_stop();    // 请求线程停止
      sub_thread_.join();            // 等待线程结束
    }
  }

  /**
   * @brief 析构函数
   * @details 确保订阅线程在对象销毁前停止。
   */
  ~SubManager() { stop(); }

 private:
  zmq::socket_t socket_;              // ZeroMQ SUB套接字
  ConnectionManager &conn_mgr_;       // 连接管理器引用
  std::jthread sub_thread_;           // 订阅线程
};

#endif  // SUB_MANAGER_HPP
