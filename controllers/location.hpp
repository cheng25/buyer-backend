/**
 * @file location.hpp
 * @brief 位置管理控制器头文件
 * @details 提供位置相关的HTTP接口，包括添加位置、获取位置集群、查找附近位置和重新聚类等功能。
 */

#pragma once

#include <drogon/HttpController.h>    // 引入Drogon HTTP控制器头文件
#include <drogon/orm/DbClient.h>      // 引入Drogon数据库客户端头文件

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
 * @class LocationController
 * @brief 位置管理控制器类
 * @details 处理位置相关的HTTP请求，包括位置数据的管理和地理聚类功能。
 */
class LocationController : public drogon::HttpController<LocationController> {
 public:
  METHOD_LIST_BEGIN
  // 添加位置接口：POST /api/v1/location，需要认证
  ADD_METHOD_TO(LocationController::add_location, "/api/v1/location",
                drogon::Post, drogon::Options, "CorsMiddleware",
                "AuthMiddleware");
  // 获取位置集群接口：GET /api/v1/location/clusters，需要认证
  ADD_METHOD_TO(LocationController::get_clusters, "/api/v1/location/clusters",
                drogon::Get, drogon::Options, "CorsMiddleware",
                "AuthMiddleware");
  // 查找附近位置接口：GET /api/v1/location/nearby，需要认证
  ADD_METHOD_TO(LocationController::find_nearby, "/api/v1/location/nearby",
                drogon::Get, drogon::Options, "CorsMiddleware",
                "AuthMiddleware");
  // 重新聚类接口：POST /api/v1/location/recluster，需要认证
  ADD_METHOD_TO(LocationController::recluster, "/api/v1/location/recluster",
                drogon::Post, drogon::Options, "CorsMiddleware",
                "AuthMiddleware");
  METHOD_LIST_END

  /**
   * @brief 添加位置方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> add_location(
      const drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);
  // Paginated by offset (15 per page)
  /**
   * @brief 获取位置集群方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 按偏移量分页获取位置集群（每页15个）。
   */
  static drogon::Task<> get_clusters(
      const drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);
  // Paginated by offset (15 per page)
  /**
   * @brief 查找附近位置方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 按偏移量分页查找附近位置（每页15个）。
   */
  static drogon::Task<> find_nearby(
      const drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 重新聚类方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 对位置数据进行重新聚类处理。
   */
  static drogon::Task<> recluster(
      const drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);
};
}  // namespace v1
}  // namespace api
