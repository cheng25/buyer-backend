/**
 * @file s3_service.cpp
 * @brief S3服务实现文件
 * @details 实现与S3兼容存储（如MinIO）的交互功能，包括预签名URL生成、媒体处理、Bucket管理和媒体信息获取。
 */
#include "s3_service.hpp"

#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/utils/DateTime.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/s3/model/GetObjectAttributesRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadBucketRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include "../../config/config.hpp"

// Windows SDK兼容性修复
#ifdef GetObject
#undef GetObject
#endif

/**
 * @brief S3Service构造函数
 * @details 初始化S3客户端，从配置文件读取MinIO端点、访问密钥和秘密密钥，
 * 配置客户端参数并创建S3客户端实例。
 */
S3Service::S3Service() {
  Aws::Client::ClientConfiguration config;

  // 从Drogon自定义配置获取配置项
  std::string endpoint =
      config::get_config_value("minio_endpoint", "http://localhost:9000");  // MinIO端点
  std::string access_key =
      config::get_config_value("minio_access_key", "minioadmin");          // 访问密钥
  std::string secret_key =
      config::get_config_value("minio_secret_key", "minioadmin");          // 秘密密钥

  config.endpointOverride = endpoint;                                      // 设置端点覆盖
  config.scheme = endpoint.starts_with("https") ? Aws::Http::Scheme::HTTPS
                                                : Aws::Http::Scheme::HTTP;  // 根据端点协议选择HTTP/HTTPS
  config.verifySSL = false;                                                // 禁用SSL验证

  LOG_INFO << "Initializing S3 client with endpoint: " << endpoint;

  // 创建S3客户端实例
  s3_client_ = std::make_unique<Aws::S3::S3Client>(
      Aws::Auth::AWSCredentials(access_key, secret_key), config,
      Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
}

/**
 * @brief 生成预签名URL
 * @details 根据指定的HTTP方法生成S3对象的预签名URL，支持GET（下载）和PUT（上传）操作。
 * @param bucket_name Bucket名称
 * @param object_key 对象键
 * @param method HTTP方法（Get或Put）
 * @param content_type 内容类型（仅PUT请求需要）
 * @param expiration_sec URL过期时间（秒）
 * @return Task<std::string> 预签名URL字符串，不支持的方法返回空字符串
 */
drogon::Task<std::string> S3Service::generate_presigned_url(
    const std::string &bucket_name, const std::string &object_key,
    drogon::HttpMethod method, const std::string &content_type,
    long long expiration_sec) {
  Aws::Http::HeaderValueCollection headers;  // HTTP请求头集合
  std::string url;                           // 生成的预签名URL

  // GET请求：生成下载URL
  if (method == drogon::HttpMethod::Get) {
    url = s3_client_->GeneratePresignedUrl(bucket_name, object_key,
                                           Aws::Http::HttpMethod::HTTP_GET,
                                           expiration_sec);
    co_return url;
  } 
  // PUT请求：生成上传URL
  else if (method == drogon::HttpMethod::Put) {
    headers["Content-Type"] = content_type;  // 设置Content-Type以提高浏览器兼容性
    url = s3_client_->GeneratePresignedUrl(bucket_name, object_key,
                                           Aws::Http::HttpMethod::HTTP_PUT,
                                           headers, expiration_sec);
    LOG_INFO << "Generated URL: " << url;
    co_return url;
  }
  // 不支持的HTTP方法
  LOG_ERROR << "Unsupported HTTP method: " << drogon::to_string(method);
  co_return url;
}

/**
 * @brief 处理媒体文件
 * @details 获取并处理指定的媒体文件，根据文件类型（图片或视频）进行相应的处理逻辑。
 * 当前实现为基础框架，后续可扩展文件大小限制、内容扫描、元数据验证、病毒扫描和格式验证等功能。
 * @param bucket_name Bucket名称
 * @param object_key 对象键
 * @return Task<bool> 处理成功返回true，失败返回false
 */
drogon::Task<bool> S3Service::process_media(const std::string &bucket_name,
                                            const std::string &object_key) {
  LOG_INFO << "Processing media: " << bucket_name << "/" << object_key;
  Aws::S3::Model::GetObjectRequest get_request;  // 获取对象请求
  get_request.SetBucket(bucket_name);            // 设置Bucket名称
  get_request.SetKey(object_key);                // 设置对象键

// Windows SDK兼容性修复
#ifdef GetObject
#undef GetObject
#endif
  // 执行获取对象操作
  Aws::S3::Model::GetObjectOutcome outcome = s3_client_->GetObject(get_request);
  if (!outcome.IsSuccess()) {
    LOG_ERROR << "Failed to get object: " << outcome.GetError().GetMessage();
    co_return false;
  }

  LOG_INFO << "Successfully retrieved object, size: "
           << outcome.GetResult().GetContentLength() << " bytes";

  auto content_type = outcome.GetResult().GetContentType();  // 获取内容类型

  /**
   * Todo:
   * media processing logic:
   * - File size limits
   * - Content scanning for inappropriate material
   * - Metadata validation
   * - Virus scanning
   * - Format validation
   */
  // auto &metadata = outcome.GetResult().GetMetadata();

  // 图片文件处理
  if (content_type.find("image/") != std::string::npos) {
    // 或特定格式，例如 content_type.ends_with(".png") || content_type.ends_with(".jpg")
    LOG_INFO << "Processing image file: "
             << object_key.substr(object_key.find("_") + 1);
    // 图片特定处理逻辑

    co_return true;
  } 
  // 视频文件处理
  else if (content_type.find("video/") != std::string::npos) {
    LOG_INFO << "Processing video file: "
             << object_key.substr(object_key.find("_") + 1);
    // 视频特定处理逻辑

    co_return true;
  } 
  // 不支持的文件类型
  else {
    LOG_INFO << "No processing for this file-type";
  }

  co_return false;
}

/**
 * @brief 确保Bucket存在
 * @details 检查指定的Bucket是否存在，如果不存在则创建新的Bucket。
 * @param bucket_name Bucket名称
 * @return Task<bool> Bucket存在或创建成功返回true，创建失败返回false
 */
drogon::Task<bool> S3Service::ensure_bucket_exists(
    const std::string &bucket_name) {
  LOG_INFO << "Checking if bucket exists: " << bucket_name;

  Aws::S3::Model::HeadBucketRequest request;  // 检查Bucket请求
  request.SetBucket(bucket_name);             // 设置Bucket名称

  // 执行HeadBucket操作检查Bucket是否存在
  auto outcome = s3_client_->HeadBucket(request);
  if (outcome.IsSuccess()) {
    LOG_INFO << "Bucket " << bucket_name << " already exists";
    co_return true;
  }

  LOG_INFO << "Bucket " << bucket_name << " does not exist, creating...";

  Aws::S3::Model::CreateBucketRequest create_request;  // 创建Bucket请求
  create_request.SetBucket(bucket_name);               // 设置Bucket名称

  // 执行CreateBucket操作创建Bucket
  auto create_outcome = s3_client_->CreateBucket(create_request);
  if (!create_outcome.IsSuccess()) {
    LOG_ERROR << "Failed to create bucket: "
              << create_outcome.GetError().GetMessage();
    co_return false;
  }

  LOG_INFO << "Created bucket: " << bucket_name;
  co_return true;
}

/**
 * @brief 获取媒体信息
 * @details 通过HeadObject操作获取指定对象的元数据信息，包括内容类型、大小、最后修改时间、ETag等。
 * @param bucket_name Bucket名称（字符串视图）
 * @param object_key 对象键
 * @return Task<MediaInfo> 媒体信息结构体，获取失败返回空结构体
 */
drogon::Task<MediaInfo> S3Service::get_media_info(
    std::string_view bucket_name, const std::string &object_key) {
  Aws::S3::Model::HeadObjectRequest head_request;  // HeadObject请求
  head_request.SetBucket(bucket_name);             // 设置Bucket名称
  head_request.SetKey(object_key);                 // 设置对象键

  // 执行HeadObject操作获取对象元数据
  auto outcome = s3_client_->HeadObject(head_request);
  if (!outcome.IsSuccess()) {
    LOG_ERROR << "Failed to get object info: "
              << outcome.GetError().GetMessage();
    co_return MediaInfo{};  // 返回空结构体表示失败
  }

  auto &result = outcome.GetResult();  // 获取操作结果
  MediaInfo info;                      // 媒体信息结构体
  info.object_key = object_key;        // 对象键
  info.content_type = result.GetContentType();    // 内容类型
  info.content_length = result.GetContentLength(); // 内容长度
  info.last_modified =
      result.GetLastModified().ToGmtString(Aws::Utils::DateFormat::ISO_8601);  // 最后修改时间（ISO8601格式）
  info.etag = result.GetETag();        // ETag

  // 遍历自定义元数据
  auto metadata = result.GetMetadata();
  for (const auto &pair : metadata) {
    info.custom_metadata[pair.first] = pair.second;
  }

  co_return info;
}
