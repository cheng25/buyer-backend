/**
 * @file chats.hpp
 * @brief 聊天消息控制器头文件
 * @details 提供聊天对话和消息相关的HTTP接口，包括获取对话列表、发送消息、标记已读等功能。
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

/**
 * @class Chats
 * @brief 聊天消息控制器类
 * @details 处理聊天对话和消息相关的HTTP请求。
 */
class Chats : public drogon::HttpController<Chats> {
 public:
  METHOD_LIST_BEGIN
  // 获取对话列表接口：GET /api/v1/conversations，需要认证
  ADD_METHOD_TO(Chats::get_conversations, "/api/v1/conversations", Get, Options,
                "CorsMiddleware", "AuthMiddleware");
  // 创建对话接口：POST /api/v1/conversations，需要认证
  ADD_METHOD_TO(Chats::create_conversation, "/api/v1/conversations", Post,
                Options, "CorsMiddleware", "AuthMiddleware");
  // 获取对话消息列表接口：GET /api/v1/conversations/{conversation_id}/messages，需要认证
  ADD_METHOD_TO(Chats::get_messages,
                "/api/v1/conversations/{conversation_id}/messages", Get,
                Options, "CorsMiddleware", "AuthMiddleware");
  // 发送消息接口：POST /api/v1/conversations/{conversation_id}/messages，需要认证
  ADD_METHOD_TO(Chats::send_message,
                "/api/v1/conversations/{conversation_id}/messages", Post,
                Options, "CorsMiddleware", "AuthMiddleware");

  // 根据报价获取对话接口：GET /api/v1/conversations/offer/{offer_id}，需要认证
  ADD_METHOD_TO(Chats::get_conversation_by_offer,
                "/api/v1/conversations/offer/{offer_id}", Get, Options,
                "CorsMiddleware", "AuthMiddleware");

  // 标记消息已读接口：POST /api/v1/conversations/{conversation_id}/read，需要认证
  ADD_METHOD_TO(Chats::mark_messages_as_read,
                "/api/v1/conversations/{conversation_id}/read", Post, Options,
                "CorsMiddleware", "AuthMiddleware");

  // 获取未读消息数量接口：GET /api/v1/conversations/unread，需要认证
  ADD_METHOD_TO(Chats::get_unread_count, "/api/v1/conversations/unread", Get,
                Options, "CorsMiddleware", "AuthMiddleware");

  METHOD_LIST_END

  /**
   * @brief 获取对话列表方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 获取当前用户参与的所有对话列表。
   */
  static drogon::Task<> get_conversations(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 创建对话方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 创建新的聊天对话。
   */
  static drogon::Task<> create_conversation(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  /**
   * @brief 获取对话消息列表方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param conversation_id 对话ID
   * @details 获取指定对话的消息列表。
   */
  static drogon::Task<> get_messages(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback,
      std::string conversation_id);

  /**
   * @brief 发送消息方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param conversation_id 对话ID
   * @details 向指定对话发送消息。
   */
  static drogon::Task<> send_message(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback,
      std::string conversation_id);

  /**
   * @brief 根据报价获取对话方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param offer_id 报价ID
   * @details 根据报价ID获取相关对话。
   */
  static drogon::Task<> get_conversation_by_offer(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback,
      std::string offer_id);

  /**
   * @brief 标记消息已读方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @param conversation_id 对话ID
   * @details 将指定对话的消息标记为已读。
   */
  static drogon::Task<> mark_messages_as_read(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback,
      std::string conversation_id);

  /**
   * @brief 获取未读消息数量方法
   * @param req HTTP请求指针
   * @param callback 响应回调函数
   * @details 获取当前用户未读消息的总数。
   */
  static drogon::Task<> get_unread_count(
      drogon::HttpRequestPtr req,
      std::function<void(const drogon::HttpResponsePtr&)> callback);
};
}  // namespace v1
}  // namespace api
