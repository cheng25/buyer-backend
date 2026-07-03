/**
 * @file AuthMiddleware.cc
 * @brief JWT认证中间件实现文件
 * @details 实现基于JWT令牌的HTTP请求认证，验证请求中的Bearer令牌，
 *          提取用户ID并添加到请求属性中，供后续控制器使用。
 */

#include <drogon/HttpMiddleware.h>    // 引入Drogon HTTP中间件头文件
// 确保了 jwt-cpp 使用 JsonCpp 后端，而不是默认的 picojson 解析器。
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
 * @class AuthMiddleware
 * @brief JWT认证中间件类
 * @details 处理HTTP请求的JWT令牌认证，验证令牌有效性并提取用户信息。
 */
class AuthMiddleware : public drogon::HttpCoroMiddleware<AuthMiddleware> {
 public:
  //AuthMiddleware() = default;
  AuthMiddleware() {};// 不要使用 = default;ERROR middleware not found - MiddlewaresFunction.cc:164 NOLINT(*-use-equals-default)

  /**
   * @brief 中间件调用方法
   * @param req HTTP请求指针
   * @param next 下一个中间件/控制器的等待器
   * @return HTTP响应指针
   * @details 验证请求中的JWT令牌，跳过OPTIONS请求，验证成功后将用户ID添加到请求属性中。
   */
  drogon::Task<drogon::HttpResponsePtr> invoke(
      const drogon::HttpRequestPtr &req,
      drogon::MiddlewareNextAwaiter &&next) override {
    // Skip OPTIONS requests (which is used for CORS) 跳过OPTIONS请求(这是用于跨域资源共享（CORS）的。)
    if (req->getMethod() == drogon::HttpMethod::Options) {
      auto resp = co_await next;    // 直接传递OPTIONS请求到下一个中间件
      co_return resp;
    }
    try {
      // Authorization header with Bearer prefix 许可头前缀
      const std::string &auth_header = req->getHeader("Authorization");    // 获取Authorization请求头

      // 检查Authorization头是否存在且以Bearer开头
      if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
        // 返回401未授权响应
        auto resp = HttpResponse::newHttpResponse(drogon::k401Unauthorized,
                                                  drogon::CT_APPLICATION_JSON);
        SimpleError err{.error = "Unauthorized: No valid token provided"};
        resp->setBody(glz::write_json(err).value_or(""));
        co_return resp;
      }
      std::string token = auth_header.substr(7);    // 提取Bearer令牌

      using traits = jwt::traits::open_source_parsers_jsoncpp;
      auto decoded = jwt::decode<traits>(token);    // 解码JWT令牌

      // Verify signature and expiration 验证签名和过期
      auto verifier =
          jwt::verify<traits>()
              .allow_algorithm(jwt::algorithm::hs256{config::JWT_SECRET})    // 使用HS256算法验证签名
              .with_issuer("buyer-app");    // 验证签发者

      // Extract user ID from token and add to request attributes for later use 提取用户ID并添加到请求属性中以备后用
      verifier.verify(decoded);    // 执行令牌验证
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

      // Pass user_id through request attributes 通过请求属性传递user_id
      req->getAttributes()->insert("current_user_id", user_id);    // 将user_id添加到请求属性

      // Token is valid, proceed to the next middleware/controller 令牌有效，继续进行下一个中间件/控制器
      auto resp = co_await next;    // 传递到下一个中间件或控制器
      co_return resp;
    } catch (const std::exception &e) {
      LOG_ERROR << "Auth error: " << e.what();    // 记录认证错误日志
      // 返回401未授权响应
      auto resp = HttpResponse::newHttpResponse(drogon::k401Unauthorized,
                                                drogon::CT_APPLICATION_JSON);
      SimpleError err{.error = "Unauthorized: Invalid token"};
      resp->setBody(glz::write_json(err).value_or(""));
      co_return resp;
    }
  }
};
