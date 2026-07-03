/**
 * @file dashboard.hpp
 * @brief 仪表盘控制器头文件
 * @details 提供仪表盘数据相关的HTTP接口，用于展示用户统计信息。
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
 * @class Dashboard
 * @brief 仪表盘控制器类
 * @details 处理仪表盘数据相关的HTTP请求。
 */
class Dashboard : public drogon::HttpController<Dashboard> {
 public:
  METHOD_LIST_BEGIN
  // 获取仪表盘数据接口：GET /api/v1/dashboard，需要认证
  ADD_METHOD_TO(Dashboard::get_dashboard_data, "/api/v1/dashboard", drogon::Get,
                drogon::Options, "CorsMiddleware", "AuthMiddleware");
  METHOD_LIST_END

  /**
   * @brief 获取仪表盘数据方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 获取当前用户的仪表盘统计数据。
   */
  static drogon::Task<> get_dashboard_data(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);
};
}  // namespace v1
}  // namespace api
