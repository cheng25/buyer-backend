/**
 * @file orders.hpp
 * @brief 订单管理控制器头文件
 * @details 提供订单相关的HTTP接口，包括获取订单列表和创建订单功能。
 */

#pragma once

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
 * @class Orders
 * @brief 订单管理控制器类
 * @details 处理订单相关的HTTP请求。
 */
class Orders : public drogon::HttpController<Orders> {
 public:
  METHOD_LIST_BEGIN
  // 获取订单列表接口：GET /api/v1/orders，需要认证
  ADD_METHOD_TO(Orders::get_orders, "/api/v1/orders", drogon::Get,
                drogon::Options, "CorsMiddleware", "AuthMiddleware");
  // 创建订单接口：POST /api/v1/orders，需要认证
  ADD_METHOD_TO(Orders::create_order, "/api/v1/orders", drogon::Post,
                drogon::Options, "CorsMiddleware", "AuthMiddleware");

  METHOD_LIST_END

  /**
   * @brief 获取订单列表方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 获取当前用户的订单列表。
   */
  static drogon::Task<> get_orders(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 创建订单方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 创建新订单。
   */
  static drogon::Task<> create_order(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);
};
}  // namespace v1
}  // namespace api
