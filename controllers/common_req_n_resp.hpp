/**
 * @file common_req_n_resp.hpp
 * @brief 通用请求与响应结构体头文件
 * @details 定义项目中使用的通用请求和响应数据结构，包括状态响应、错误响应、媒体信息和通知消息等。
 */

#ifndef COMMON_REQ_N_RESP_HPP
#define COMMON_REQ_N_RESP_HPP

#include <optional>                 // 引入可选类型头文件
#include <string>                   // 引入字符串类头文件
#include <vector>                   // 引入向量容器头文件

/**
 * @struct StatusResponse
 * @brief 状态响应结构体
 * @details 包含状态和消息的通用响应结构。
 */
struct StatusResponse {
  std::string status;           // 状态
  std::string message;          // 消息
};

/**
 * @struct SimpleStatus
 * @brief 简单状态响应结构体
 * @details 仅包含状态的简化响应结构。
 */
struct SimpleStatus {
  std::string status;           // 状态
};

/**
 * @struct SimpleError
 * @brief 简单错误响应结构体
 * @details 仅包含错误信息的简化错误响应结构。
 */
struct SimpleError {
  std::string error;            // 错误信息
};

/**
 * @struct MediaQuickInfo
 * @brief 媒体快速信息结构体
 * @details 存储媒体文件的基本信息，用于快速展示和传输。
 */
struct MediaQuickInfo {
  int media_id;                 // 媒体ID
  std::string object_key;       // 对象键（S3存储键）
  std::string filename;         // 文件名
  std::string mime_type;        // MIME类型
  int64_t size = 0;             // 文件大小（字节）
  // No metadata for now 暂无元数据
};

/**
 * @struct MediaInput
 * @brief 媒体输入结构体
 * @details 存储媒体文件的输入信息，用于上传和处理。
 */
struct MediaInput {
  std::vector<std::string> object_keys;// 对象键列表（S3存储键）
};

/**
 * @struct DeleteMediaRequest
 * @brief 删除媒体请求结构体
 * @details 存储要删除的媒体文件ID列表。
 */
struct DeleteMediaRequest {
  std::vector<int> media_ids;     // 要删除的媒体ID列表
};

/**
 * @struct MediaResponse
 * @brief 媒体响应结构体
 * @details 存储媒体处理后的响应信息。
 */
struct MediaResponse {
  std::vector<int> media_ids;     // 媒体ID列表
};

/**
 * @struct MediaInfoResponse
 * @brief 媒体信息响应结构体
 * @details 存储媒体文件的详细信息列表。
 */
struct MediaInfoResponse {
  std::vector<MediaQuickInfo> media;// 媒体信息集合
};

/**
 * @struct NotificationMessage
 * @brief 通知消息结构体
 * @details 存储实时通知消息的数据结构，用于WebSocket推送。
 */
struct NotificationMessage {
  std::string type;                 // 通知类型（如chat_created、message_sent等）
  std::string id;                   // 关联对象ID（对话ID、帖子ID等）
  std::string message;              // 通知消息内容
  std::string modified_at;          // 修改时间
};

#endif  // COMMON_REQ_N_RESP_HPP
