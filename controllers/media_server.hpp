/**
 * @file media_server.hpp
 * @brief 媒体服务控制器头文件
 * @details 提供媒体文件相关的HTTP接口，包括获取上传URL、验证证明、获取媒体URL和元数据等功能。
 */

#ifndef MEDIA_SERVER_CONTROLLER_HPP
#define MEDIA_SERVER_CONTROLLER_HPP

#include <drogon/HttpController.h>    // 引入Drogon HTTP控制器头文件

/**
 * @namespace api
 * @brief API命名空间
 */
namespace api {

/**
 * @namespace v1
 * @brief API版本1命名空间
 */
namespace v1 {

/**
 * @brief Media server controller 媒体服务控制器
 * @note query params used because object_key contains '/' character
 *       使用查询参数是因为object_key包含'/'字符
 */
class MediaController : public drogon::HttpController<MediaController> {
 public:
  METHOD_LIST_BEGIN
  // 获取上传URL接口：POST /api/v1/media/upload-url，需要认证
  ADD_METHOD_TO(MediaController::get_upload_url, "/api/v1/media/upload-url",
                drogon::Post, drogon::Options, "CorsMiddleware",
                "AuthMiddleware");
  // 验证证明接口：GET /api/v1/media/verify-proof，需要认证
  ADD_METHOD_TO(MediaController::verify_proof, "/api/v1/media/verify-proof",
                drogon::Get, drogon::Options, "CorsMiddleware",
                "AuthMiddleware");
  // 获取媒体URL接口：GET /api/v1/media，需要认证
  ADD_METHOD_TO(MediaController::get_media_url, "/api/v1/media", drogon::Get,
                drogon::Options, "CorsMiddleware", "AuthMiddleware");
  // 验证对象键接口：GET /api/v1/media/verify，需要认证
  ADD_METHOD_TO(MediaController::verify_object_key, "/api/v1/media/verify",
                drogon::Get, drogon::Options, "CorsMiddleware",
                "AuthMiddleware");
  // 获取媒体元数据接口：GET /api/v1/media/metadata，需要认证
  ADD_METHOD_TO(MediaController::get_media_metadata, "/api/v1/media/metadata",
                drogon::Get, drogon::Options, "CorsMiddleware",
                "AuthMiddleware");
  METHOD_LIST_END

  /**
   * @brief 获取上传URL方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> get_upload_url(
      const drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  /**
   * @brief 验证证明方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> verify_proof(
      const drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  /**
   * @brief 获取媒体URL方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> get_media_url(
      const drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  /**
   * @brief 验证对象键方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> verify_object_key(
      const drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  /**
   * @brief 获取媒体元数据方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> get_media_metadata(
      const drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr &)> callback);
};

}  // namespace v1

}  // namespace api

#endif MEDIA_CONTROLLER_HPP  // MEDIA_CONTROLLER_HPP
