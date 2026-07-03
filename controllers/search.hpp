/**
 * @file search.hpp
 * @brief 搜索控制器头文件
 * @details 提供搜索功能的HTTP接口，支持对帖子等资源进行搜索。
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
 * @class Search
 * @brief 搜索控制器类
 * @details 处理搜索相关的HTTP请求。
 */
class Search : public drogon::HttpController<Search> {
 public:
  METHOD_LIST_BEGIN
  // 搜索接口：GET /api/v1/search，需要认证
  ADD_METHOD_TO(Search::search, "/api/v1/search", drogon::Get, drogon::Options,
                "CorsMiddleware", "AuthMiddleware");

  METHOD_LIST_END

  /**
   * @brief 搜索方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 根据搜索关键词搜索相关资源。
   */
  static drogon::Task<> search(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);
};
}  // namespace v1
}  // namespace api
