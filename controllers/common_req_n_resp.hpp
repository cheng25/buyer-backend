#ifndef COMMON_REQ_N_RESP_HPP
#define COMMON_REQ_N_RESP_HPP

#include <optional>
#include <string>
#include <vector>

// 状态响应
struct StatusResponse {
  std::string status;           // 状态
  std::string message;          // 消息
};

// 简单状态响应
struct SimpleStatus {
  std::string status;           // 状态
};

// 简单错误
struct SimpleError {
  std::string error;            // 错误
};

// 媒体信息
struct MediaQuickInfo {
  int media_id;                 // 媒体ID
  std::string object_key;       // 对象键
  std::string filename;         // 文件名
  std::string mime_type;        // MIME类型
  int64_t size = 0;             // 大小
  // No metadata for now 暂无元数据
};

// 媒体输入
struct MediaInput {
  std::vector<std::string> object_keys;// 对象键
};

// 删除媒体请求
struct DeleteMediaRequest {
  std::vector<int> media_ids;     // 媒体ID
};

// 媒体响应
struct MediaResponse {
  std::vector<int> media_ids;     // 媒体ID
};

// 媒体信息响应
struct MediaInfoResponse {
  std::vector<MediaQuickInfo> media;// 媒体信息集合
};

// 通知消息
struct NotificationMessage {
  std::string type;                 // 类型
  std::string id;                   // ID
  std::string message;              // 消息
  std::string modified_at;          // 修改时间
};

#endif  // COMMON_REQ_N_RESP_HPP
