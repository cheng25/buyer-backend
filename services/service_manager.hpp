/**
 * @file service_manager.hpp
 * @brief 服务管理器头文件
 * @details 提供全局服务管理功能，使用单例模式管理所有服务实例，
 *          包括发布者、订阅者、连接管理器和S3服务。
 */

#ifndef SERVICE_MANAGER_HPP
#define SERVICE_MANAGER_HPP

#include <memory>    // 引入智能指针头文件
#include <zmq.hpp>   // 引入ZeroMQ消息队列头文件

#include "./media_server/s3_service.hpp"    // 引入S3服务头文件
#include "./subber/connection_manager.hpp"  // 引入连接管理器头文件
#include "./subber/pub_manager.hpp"         // 引入发布管理器头文件
#include "./subber/sub_manager.hpp"         // 引入订阅管理器头文件

// Redis PubSub option: noticed instability with large number of subscriptions
// Redis发布订阅选项：在大量订阅时发现不稳定问题
// #include "./subber/redis_pub_manager.hpp"
// #include "./subber/redis_sub_manager.hpp"

/**
 * @namespace service
 * @brief 服务命名空间
 * @details 包含服务相关的常量定义。
 */
namespace service {
inline constexpr std::string_view BUCKET_NAME = "media";    // 媒体存储桶名称
inline constexpr std::uint32_t MAX_MEDIA_SIZE = 5U;         // 最大媒体文件大小（MB）
}  // namespace service

/**
 * @class ServiceManager
 * @brief 服务管理器类
 * @details 使用单例模式管理所有全局服务，包括发布者、订阅者、连接管理器和S3服务。
 */
class ServiceManager {
 public:
  /**
   * @brief 获取单例实例
   * @return ServiceManager引用
   */
  static ServiceManager& get_instance() {
    static ServiceManager instance;
    return instance;
  }

  // non-copyable, move-only 禁止拷贝，仅支持移动
  ServiceManager(const ServiceManager&) = delete;
  ServiceManager& operator=(const ServiceManager&) = delete;

  /**
   * @brief 获取发布管理器
   * @return PubManager引用
   */
  PubManager& get_publisher() { return *publisher_; }

  /**
   * @brief 获取订阅管理器
   * @return SubManager引用
   */
  SubManager& get_subscriber() { return *subscriber_; }

  /**
   * @brief 获取连接管理器
   * @return ConnectionManager引用
   */
  ConnectionManager& get_connection_manager() { return *conn_mgr_; }

  /**
   * @brief 获取S3服务
   * @return S3Service引用
   */
  S3Service& get_s3_service() { return *s3_service_; }

  /**
   * @brief 初始化所有服务
   * @details 创建ZeroMQ上下文、连接管理器、发布者、订阅者和S3服务，
   *          初始化AWS SDK并启动订阅者线程。
   */
  void initialize() {
    context_ = std::make_unique<zmq::context_t>(1);    // 创建ZeroMQ上下文
    conn_mgr_ = std::make_unique<ConnectionManager>();    // 创建连接管理器
    publisher_ = std::make_unique<PubManager>(*context_);    // 创建发布管理器
    subscriber_ = std::make_unique<SubManager>(*context_, *conn_mgr_);    // 创建订阅管理器

    // AWS SDK 初始化
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    s3_service_ = std::make_unique<S3Service>();    // 创建S3服务

    // // Redis PubSub option:
    // conn_mgr_ = std::make_unique<ConnectionManager>();
    // publisher_ = std::make_unique<PubManager>("tcp://127.0.0.1:6379");
    // // publisher_ = std::make_unique<PubManager>("tcp://127.0.0.1:6400"); //
    // // valkey option
    // subscriber_ = std::make_unique<SubManager>(
    //     publisher_->get_redis_subscriber(), *conn_mgr_);

    // Start subscriber 启动订阅者线程
    subscriber_->run();
  }

  /**
   * @brief 关闭所有服务
   * @details 优雅停止订阅者线程，关闭AWS SDK。
   */
  void shutdown() {
    if (subscriber_) {
      subscriber_->stop();  // Graceful shutdown 优雅关闭订阅者
    }

    Aws::SDKOptions options;
    Aws::ShutdownAPI(options);    // 关闭AWS SDK
  }

 private:
  ServiceManager() = default;    // 默认私有构造函数

  std::unique_ptr<zmq::context_t> context_;    // ZeroMQ上下文
  std::unique_ptr<ConnectionManager> conn_mgr_;    // 连接管理器
  std::unique_ptr<PubManager> publisher_;    // 发布管理器
  std::unique_ptr<SubManager> subscriber_;    // 订阅管理器
  std::unique_ptr<S3Service> s3_service_;    // S3服务
};

#endif  // SERVICE_MANAGER_HPP
