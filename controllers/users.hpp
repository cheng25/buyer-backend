/**
 * @file users.hpp
 * @brief 用户管理控制器头文件
 * @details 提供用户相关的HTTP接口，包括获取用户列表等功能。
 */

#ifndef USERS_HPP
#define USERS_HPP

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
 * @class Users
 * @brief 用户管理控制器类
 * @details 处理用户相关的HTTP请求。
 */
class Users : public drogon::HttpController<Users> {
 public:
  METHOD_LIST_BEGIN
  // 获取用户列表接口：GET /api/v1/users，需要认证
  ADD_METHOD_TO(Users::get_users, "/api/v1/users", drogon::Get, drogon::Options,
                "CorsMiddleware", "AuthMiddleware");
  METHOD_LIST_END

  /**
   * @brief 获取用户列表方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 获取系统中的用户列表。
   */
  static drogon::Task<> get_users(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);
};

}  // namespace v1
}  // namespace api

#endif  // USERS_HPP