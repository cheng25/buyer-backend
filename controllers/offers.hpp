/**
 * @file offers.hpp
 * @brief 报价管理控制器头文件
 * @details 提供报价相关的HTTP接口，包括创建报价、接受/拒绝报价、协商、证明提交和托管等功能。
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

// Drogon框架类型别名，简化代码
using drogon::Get;
using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using drogon::Options;
using drogon::Post;
using drogon::Put;

/**
 * @class Offers
 * @brief 报价管理控制器类
 * @details 处理报价相关的HTTP请求，包括报价的创建、查看、更新、接受/拒绝、协商、证明和托管等操作。
 */
class Offers : public drogon::HttpController<Offers> {
 public:
  METHOD_LIST_BEGIN
  // Get all offers for a post 获取帖子的所有报价
  ADD_METHOD_TO(Offers::get_offers_for_post, "/api/v1/posts/{post_id}/offers",
                Get, Options, "CorsMiddleware", "AuthMiddleware");

  // Create a new offer for a post 为帖子创建新报价
  ADD_METHOD_TO(Offers::create_offer, "/api/v1/posts/{post_id}/offers", Post,
                Options, "CorsMiddleware", "AuthMiddleware");

  // 获取单个报价详情
  ADD_METHOD_TO(Offers::get_offer, "/api/v1/offers/{id}", Get, Options,
                "CorsMiddleware", "AuthMiddleware");

  // 更新报价信息
  ADD_METHOD_TO(Offers::update_offer, "/api/v1/offers/{id}", Put, Options,
                "CorsMiddleware", "AuthMiddleware");

  // Accept an offer 接受报价
  ADD_METHOD_TO(Offers::accept_offer, "/api/v1/offers/{id}/accept", Post,
                Options, "CorsMiddleware", "AuthMiddleware");

  // 接受反报价
  ADD_METHOD_TO(Offers::accept_counter_offer,
                "/api/v1/offers/{id}/accept-counter", Post, Options,
                "CorsMiddleware", "AuthMiddleware");

  // Reject an offer 拒绝报价
  ADD_METHOD_TO(Offers::reject_offer, "/api/v1/offers/{id}/reject", Post,
                Options, "CorsMiddleware", "AuthMiddleware");

  // Get all offers made by the current user 获取当前用户发出的所有报价
  ADD_METHOD_TO(Offers::get_my_offers, "/api/v1/offers/my-offers", Get, Options,
                "CorsMiddleware", "AuthMiddleware");

  // Get all offers received for the current user's posts 获取当前用户帖子收到的所有报价
  ADD_METHOD_TO(Offers::get_received_offers, "/api/v1/offers/received", Get,
                Options, "CorsMiddleware", "AuthMiddleware");

  // Get notifications for the current user 获取当前用户的通知
  ADD_METHOD_TO(Offers::get_notifications, "/api/v1/offers/notifications", Get,
                Options, "CorsMiddleware", "AuthMiddleware");

  // Mark a notification as read 将通知标记为已读
  ADD_METHOD_TO(Offers::mark_notification_read,
                "/api/v1/offers/notifications/{id}/read", Post, Options,
                "CorsMiddleware", "AuthMiddleware");

  // 将所有通知标记为已读
  ADD_METHOD_TO(Offers::mark_all_notifications_read,
                "/api/v1/offers/notifications/read-all", Post, Options,
                "CorsMiddleware", "AuthMiddleware");

  // 协商报价
  ADD_METHOD_TO(Offers::negotiate_offer, "/api/v1/offers/{id}/negotiate", Post,
                Options, "CorsMiddleware", "AuthMiddleware");

  // 获取报价协商记录
  ADD_METHOD_TO(Offers::get_negotiations, "/api/v1/offers/{id}/negotiations",
                Get, Options, "CorsMiddleware", "AuthMiddleware");

  // 请求证明
  ADD_METHOD_TO(Offers::request_proof, "/api/v1/offers/{id}/proof/request",
                Post, Options, "CorsMiddleware", "AuthMiddleware");

  // 提交证明
  ADD_METHOD_TO(Offers::submit_proof, "/api/v1/offers/{id}/proof/submit", Post,
                Options, "CorsMiddleware", "AuthMiddleware");

  // 获取证明列表
  ADD_METHOD_TO(Offers::get_proofs, "/api/v1/offers/{id}/proofs", Get, Options,
                "CorsMiddleware", "AuthMiddleware");

  // 批准证明
  ADD_METHOD_TO(Offers::approve_proof,
                "/api/v1/offers/{id}/proof/{proof_id}/approve", Post, Options,
                "CorsMiddleware", "AuthMiddleware");

  // 拒绝证明
  ADD_METHOD_TO(Offers::reject_proof,
                "/api/v1/offers/{id}/proof/{proof_id}/reject", Post, Options,
                "CorsMiddleware", "AuthMiddleware");

  // 创建托管
  ADD_METHOD_TO(Offers::create_escrow, "/api/v1/offers/{id}/escrow", Post,
                Options, "CorsMiddleware", "AuthMiddleware");

  // 获取托管信息
  ADD_METHOD_TO(Offers::get_escrow, "/api/v1/offers/{id}/escrow", Get, Options,
                "CorsMiddleware", "AuthMiddleware");
  METHOD_LIST_END

  /**
   * @brief 获取帖子的所有报价
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param post_id 帖子ID
   */
  static drogon::Task<> get_offers_for_post(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string post_id);

  /**
   * @brief 创建新报价
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param post_id 帖子ID
   */
  static drogon::Task<> create_offer(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string post_id);

  /**
   * @brief 获取报价详情
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> get_offer(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 更新报价
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> update_offer(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 接受报价
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> accept_offer(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 接受反报价
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> accept_counter_offer(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 拒绝报价
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> reject_offer(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 获取当前用户发出的报价
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> get_my_offers(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback);

  /**
   * @brief 获取当前用户收到的报价
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> get_received_offers(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback);

  /**
   * @brief 获取通知列表
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> get_notifications(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback);

  /**
   * @brief 将通知标记为已读
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 通知ID
   */
  static drogon::Task<> mark_notification_read(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 将所有通知标记为已读
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   */
  static drogon::Task<> mark_all_notifications_read(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback);

  /**
   * @brief 协商报价
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> negotiate_offer(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 获取协商记录
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> get_negotiations(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  // upcoming features 即将推出的功能

  /**
   * @brief 请求证明
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> request_proof(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 提交证明
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> submit_proof(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 获取证明列表
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> get_proofs(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 批准证明
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   * @param proof_id 证明ID
   */
  static drogon::Task<> approve_proof(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id, std::string proof_id);

  /**
   * @brief 拒绝证明
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   * @param proof_id 证明ID
   */
  static drogon::Task<> reject_proof(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id, std::string proof_id);

  /**
   * @brief 创建托管
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> create_escrow(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);

  /**
   * @brief 获取托管信息
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param id 报价ID
   */
  static drogon::Task<> get_escrow(
      HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
      std::string id);
};
}  // namespace v1
}  // namespace api
