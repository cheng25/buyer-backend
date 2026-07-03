/**
 * @file s3_service.hpp
 * @brief S3服务头文件
 * @details 提供AWS S3存储服务的封装，支持生成预签名URL、处理媒体文件、
 *          确保存储桶存在和获取媒体信息等功能。
 */

#ifndef S3_SERVICE_HPP
#define S3_SERVICE_HPP

#include <aws/core/Aws.h>           // 引入AWS SDK核心头文件
#include <aws/s3/S3Client.h>        // 引入AWS S3客户端头文件
#include <drogon/HttpController.h>  // 引入Drogon HTTP控制器头文件
#include <drogon/utils/coroutine.h>  // 引入Drogon协程工具头文件

#include <memory>                   // 引入智能指针头文件
#include <string>                   // 引入字符串类头文件
#include <unordered_map>            // 引入无序映射容器头文件

/**
 * @struct MediaInfo
 * @brief 媒体信息结构体
 * @details 存储媒体文件的元数据信息。
 */
struct MediaInfo {
  std::string object_key;           // 媒体名（对象键）
  std::string content_type;         // 媒体类型（MIME类型）
  long long content_length = 0;     // 媒体长度（字节）
  std::string last_modified;        // 最后修改时间
  std::string etag;                 // 媒体标签（ETag）
  std::unordered_map<std::string, std::string> custom_metadata;  // 自定义元数据
};

/**
 * @class S3Service
 * @brief S3服务类
 * @details 封装AWS S3存储服务的操作，提供媒体文件的上传、下载和管理功能。
 */
class S3Service {
 public:
  /**
   * @brief 构造函数
   * @details 初始化S3客户端。
   */
  S3Service();

  /**
   * @brief 生成预签名URL
   * @param bucket_name 存储桶名称
   * @param object_key 对象键（媒体文件名）
   * @param method HTTP方法（默认为Put）
   * @param content_type 内容类型（默认为application/octet-stream）
   * @param expiration_sec URL过期时间（秒，默认3600秒）
   * @return 预签名URL字符串
   * @details 生成用于上传或下载媒体文件的预签名URL。
   */
  drogon::Task<std::string> generate_presigned_url(
      const std::string& bucket_name, const std::string& object_key,
      drogon::HttpMethod method = drogon::HttpMethod::Put,
      const std::string& content_type = "application/octet-stream",
      long long expiration_sec = 3600);

  /**
   * @brief 处理媒体文件
   * @param bucket_name 存储桶名称
   * @param object_key 对象键（媒体文件名）
   * @return 处理是否成功
   * @details 处理上传的媒体文件，包括验证和元数据提取。
   */
  drogon::Task<bool> process_media(const std::string& bucket_name,
                                   const std::string& object_key);

  /**
   * @brief 确保存储桶存在
   * @param bucket_name 存储桶名称
   * @return 存储桶是否存在或创建成功
   * @details 检查存储桶是否存在，如果不存在则创建。
   */
  drogon::Task<bool> ensure_bucket_exists(const std::string& bucket_name);

  /**
   * @brief 获取媒体信息
   * @param bucket_name 存储桶名称
   * @param object_key 对象键（媒体文件名）
   * @return MediaInfo结构体，包含媒体的元数据信息
   * @details 获取指定媒体文件的元数据信息。
   */
  drogon::Task<MediaInfo> get_media_info(std::string_view bucket_name,
                                         const std::string& object_key);

  /**
   * @brief 获取S3客户端指针
   * @return S3客户端指针
   */
  Aws::S3::S3Client* get_client() { return s3_client_.get(); }

 private:
  std::unique_ptr<Aws::S3::S3Client> s3_client_;    // S3客户端实例
};

#endif  // S3_SERVICE_HPP
