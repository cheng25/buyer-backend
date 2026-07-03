/**
 * @file authentication.hpp
 * @brief 用户认证控制器头文件
 * @details 提供用户登录、登出、刷新令牌和注册功能的HTTP接口。
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
 * @class Authentication
 * @brief 用户认证控制器类
 * @details 处理用户认证相关的HTTP请求，包括登录、登出、令牌刷新和用户注册。
 */
class Authentication : public drogon::HttpController<Authentication> {
 public:
  METHOD_LIST_BEGIN
  // 登录接口：POST /api/v1/auth/login，无需认证
  ADD_METHOD_TO(Authentication::login, "/api/v1/auth/login", drogon::Post,
                drogon::Options, "CorsMiddleware");
  // 登出接口：POST /api/v1/auth/logout，需要认证
  ADD_METHOD_TO(Authentication::logout, "/api/v1/auth/logout", drogon::Post,
                drogon::Options, "CorsMiddleware", "AuthMiddleware");
  // 刷新令牌接口：POST /api/v1/auth/refresh，无需认证
  ADD_METHOD_TO(Authentication::refresh, "/api/v1/auth/refresh", drogon::Post,
                drogon::Options, "CorsMiddleware");
  // 注册接口：POST /api/v1/auth/register，无需认证
  ADD_METHOD_TO(Authentication::register_user, "/api/v1/auth/register",
                drogon::Post, drogon::Options, "CorsMiddleware");
  METHOD_LIST_END

  /**
   * @brief 用户登录方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 处理用户登录请求，验证用户凭据并返回JWT令牌。
   */
  static drogon::Task<> login(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 用户登出方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 处理用户登出请求，使当前令牌失效。
   */
  static drogon::Task<> logout(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 刷新令牌方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 使用刷新令牌获取新的访问令牌。
   */
  static drogon::Task<> refresh(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 用户注册方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 处理新用户注册请求，创建用户账户。
   */
  static drogon::Task<> register_user(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);
};
}  // namespace v1
}  // namespace api
