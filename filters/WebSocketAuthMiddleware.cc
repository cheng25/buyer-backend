/**
 * @file WebSocketAuthMiddleware.cc
 * @brief WebSocket认证中间件实现文件
 * @details 实现WebSocket连接的JWT令牌认证，支持从查询参数或Authorization头获取令牌，
 *          验证令牌有效性并提取用户ID。
 */

#include <drogon/HttpMiddleware.h>    // 引入Drogon HTTP中间件头文件
#ifndef JWT_DISABLE_PICOJSON
#define JWT_DISABLE_PICOJSON
#endif
#include <jwt-cpp/jwt.h>    // 引入JWT库头文件
#include <jwt-cpp/traits/open-source-parsers-jsoncpp/traits.h>    // 引入JsonCpp后端的JWT traits

#include <glaze/glaze.hpp>    // 引入Glaze JSON序列化库
#include <string>             // 引入字符串类头文件

#include "../config/config.hpp"    // 引入配置管理模块
#include "../controllers/common_req_n_resp.hpp"    // 引入通用请求响应结构

using drogon::HttpResponse;    // Drogon HTTP响应类型别名

/**
 * @class WebSocketAuthMiddleware
 * @brief WebSocket认证中间件类
 * @details 处理WebSocket连接的JWT令牌认证，支持从查询参数或Authorization头获取令牌。
 */
class WebSocketAuthMiddleware
    : public drogon::HttpMiddleware<WebSocketAuthMiddleware> {
 public:
  WebSocketAuthMiddleware() {};    // 默认构造函数

  /**
   * @brief 中间件调用方法
   * @param req HTTP请求指针
   * @param nextCb 下一个中间件的回调函数
   * @param mcb 中间件完成回调函数
   * @details 验证WebSocket连接请求中的JWT令牌，支持从查询参数或Authorization头获取令牌，
   *          验证成功后将用户ID添加到请求属性中。
   */
  void invoke(const drogon::HttpRequestPtr &req,
              drogon::MiddlewareNextCallback &&nextCb,
              drogon::MiddlewareCallback &&mcb) override {
    // CORS
    if (req->getMethod() == drogon::HttpMethod::Options) {
      nextCb(std::move(mcb));    // 直接传递OPTIONS请求
      return;
    }

    try {
      // Try to get token from query parameter first 优先从查询参数获取令牌
      auto token = req->getParameter("token");
      if (token.empty()) {
        // Fallback to Authorization header 回退到Authorization头获取令牌
        const std::string &auth_header = req->getHeader("Authorization");
        if (!auth_header.empty() && auth_header.substr(0, 7) == "Bearer ") {
          token = auth_header.substr(7);
        }
      }

      // 检查令牌是否为空
      if (token.empty()) {
        LOG_ERROR << "WebSocket connection rejected: No token provided";    // 记录拒绝连接日志
        auto resp = HttpResponse::newHttpResponse(drogon::k401Unauthorized,
                                                  drogon::CT_APPLICATION_JSON);
        SimpleError err{.error = "Unauthorized: No valid token provided"};
        resp->setBody(glz::write_json(err).value_or(""));
        mcb(resp);
        return;
      }

      using traits = jwt::traits::open_source_parsers_jsoncpp;
      auto decoded = jwt::decode<traits>(token);    // 解码JWT令牌

      // 创建JWT验证器，使用HS256算法和签发者验证
      auto verifier =
          jwt::verify<traits>()
              .allow_algorithm(jwt::algorithm::hs256{config::JWT_SECRET})
              .with_issuer("buyer-app");

      verifier.verify(decoded);    // 验证令牌签名和过期时间

      verifier.verify(decoded);    // 再次验证（冗余验证）
      auto claim = decoded.get_payload_claim("user_id");    // 获取user_id声明
      auto payload_type = claim.get_type();    // 获取声明类型
      std::string user_id;
      // 根据声明类型转换user_id
      if (payload_type == jwt::json::type::integer ||
          payload_type == jwt::json::type::number)
        user_id = std::to_string(static_cast<int>(claim.as_number()));
      else if (payload_type == jwt::json::type::string)
        user_id = claim.as_string();
      else
        throw std::runtime_error("invalid type");

      req->getAttributes()->insert("current_user_id", user_id);    // 将user_id添加到请求属性

      nextCb(std::move(mcb));    // 传递到下一个中间件
    } /* catch (const jwt::error::token_verification_exception& e) {
      if(e.code() == jwt::error::token_verification_error::token_expired) {
      auto resp = HttpResponse::newHttpJsonResponse(
          {{"error", "token expired"}});
      resp->setStatusCode(k401Unauthorized);
      mcb(resp);
      } else {
        throw e;
      }
    }  */
    catch (const std::exception &e) {
      LOG_ERROR << "Websocket auth error: " << e.what();    // 记录认证错误日志
      auto resp = HttpResponse::newHttpResponse(drogon::k401Unauthorized,
                                                drogon::CT_APPLICATION_JSON);
      SimpleError err{.error = "Unauthorized: Invalid token"};
      resp->setBody(glz::write_json(err).value_or(""));
      mcb(resp);
    }
  }
};
