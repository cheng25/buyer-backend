/**
 * @file community.hpp
 * @brief 社区帖子控制器头文件
 * @details 提供社区帖子相关的HTTP接口，包括帖子的创建、查看、筛选、订阅和标签管理等功能。
 */

#pragma once

#include <drogon/HttpController.h>    // 引入Drogon HTTP控制器头文件

#include <string>                     // 引入字符串类头文件

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

// Drogon框架类型别名，简化代码
using drogon::Get;
using drogon::Options;
using drogon::Post;
using drogon::Put;

/**
 * @class Community
 * @brief 社区帖子控制器类
 * @details 处理社区帖子相关的HTTP请求，包括帖子的CRUD操作、订阅管理和标签查询等。
 */
class Community : public drogon::HttpController<Community> {
 public:
  METHOD_LIST_BEGIN
  // 获取帖子列表接口：GET /api/v1/posts，需要认证
  ADD_METHOD_TO(Community::get_posts, "/api/v1/posts", Get, Options,
                "CorsMiddleware", "AuthMiddleware");
  // 获取帖子详情接口：GET /api/v1/posts/{id}，需要认证
  ADD_METHOD_TO(Community::get_post_by_id, "/api/v1/posts/{id}", Get, Options,
                "CorsMiddleware", "AuthMiddleware");
  // 筛选帖子接口：GET /api/v1/posts/filter，需要认证
  ADD_METHOD_TO(Community::filter_posts, "/api/v1/posts/filter", Get, Options,
                "CorsMiddleware", "AuthMiddleware");
  // 获取订阅帖子列表接口：GET /api/v1/posts/subscriptions，需要认证
  ADD_METHOD_TO(Community::get_subscriptions, "/api/v1/posts/subscriptions",
                Get, Options, "CorsMiddleware", "AuthMiddleware");
  // 获取热门标签接口：GET /api/v1/posts/tags，需要认证
  ADD_METHOD_TO(Community::get_popular_tags, "/api/v1/posts/tags", Get, Options,
                "CorsMiddleware", "AuthMiddleware");

  // 创建帖子接口：POST /api/v1/posts，需要认证
  ADD_METHOD_TO(Community::create_post, "/api/v1/posts", Post, Options,
                "CorsMiddleware", "AuthMiddleware");
  // 订阅帖子接口：POST /api/v1/posts/{id}/subscribe，需要认证
  ADD_METHOD_TO(Community::subscribe_to_post, "/api/v1/posts/{id}/subscribe",
                Post, Options, "CorsMiddleware", "AuthMiddleware");
  // 取消订阅帖子接口：POST /api/v1/posts/{id}/unsubscribe，需要认证
  ADD_METHOD_TO(Community::unsubscribe_from_post,
                "/api/v1/posts/{id}/unsubscribe", Post, Options,
                "CorsMiddleware", "AuthMiddleware");

  // 订阅实体接口：POST /api/v1/entity/{name}/subscribe，需要认证
  ADD_METHOD_TO(Community::subscribe_to_entity,
                "/api/v1/entity/{name}/subscribe", Post, Options,
                "CorsMiddleware", "AuthMiddleware");
  // 取消订阅实体接口：POST /api/v1/entity/{name}/unsubscribe，需要认证
  ADD_METHOD_TO(Community::unsubscribe_from_entity,
                "/api/v1/entity/{name}/unsubscribe", Post, Options,
                "CorsMiddleware", "AuthMiddleware");

  // 更新帖子接口：PUT /api/v1/posts/{id}，需要认证
  ADD_METHOD_TO(Community::update_post, "/api/v1/posts/{id}", Put, Options,
                "CorsMiddleware", "AuthMiddleware");
  METHOD_LIST_END

  /**
   * @brief 获取帖子列表方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 获取所有帖子的分页信息流，包含帖子信息、用户信息、订阅数、当前用户是否已订阅、媒体附件。
   */
  static drogon::Task<> get_posts(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 根据帖子ID获取单个帖子方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 帖子ID
   */
  static drogon::Task<> get_post_by_id(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 筛选帖子方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 按标签、地点和状态对帖子进行筛选。
   */
  static drogon::Task<> filter_posts(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 获取当前用户订阅的帖子方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> get_subscriptions(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 获取热门标签方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 获取按使用频率排名前20的标签。
   */
  static drogon::Task<> get_popular_tags(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 创建新帖子方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> create_post(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 订阅帖子方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 帖子ID
   */
  static drogon::Task<> subscribe_to_post(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 取消订阅帖子方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 帖子ID
   */
  static drogon::Task<> unsubscribe_from_post(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 订阅实体方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param name 实体名称
   */
  static drogon::Task<> subscribe_to_entity(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback,
      std::string name);

  /**
   * @brief 取消订阅实体方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param name 实体名称
   */
  static drogon::Task<> unsubscribe_from_entity(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback,
      std::string name);

  /**
   * @brief 更新帖子方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 帖子ID
   */
  static drogon::Task<> update_post(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback,
      std::string id);
};
}  // namespace v1
}  // namespace api
