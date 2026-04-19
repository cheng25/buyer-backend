#include <drogon/HttpMiddleware.h>
// 确保了 jwt-cpp 使用 JsonCpp 后端，而不是默认的 picojson 解析器。
#ifndef JWT_DISABLE_PICOJSON
#define JWT_DISABLE_PICOJSON
#endif
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/open-source-parsers-jsoncpp/traits.h>

#include <glaze/glaze.hpp>
#include <string>

#include "../config/config.hpp"
#include "../controllers/common_req_n_resp.hpp"

using drogon::HttpResponse;

class AuthMiddleware : public drogon::HttpCoroMiddleware<AuthMiddleware> {
 public:
  //AuthMiddleware() = default;
  AuthMiddleware() {};// 不要使用 = default;ERROR middleware not found - MiddlewaresFunction.cc:164 NOLINT(*-use-equals-default)

  drogon::Task<drogon::HttpResponsePtr> invoke(
      const drogon::HttpRequestPtr &req,
      drogon::MiddlewareNextAwaiter &&next) override {
    // Skip OPTIONS requests (which is used for CORS) 跳过OPTIONS请求(这是用于跨域资源共享（CORS）的。)
    if (req->getMethod() == drogon::HttpMethod::Options) {
      auto resp = co_await next;
      co_return resp;
    }
    try {
      // Authorization header with Bearer prefix 许可头前缀
      const std::string &auth_header = req->getHeader("Authorization");

      if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
        auto resp = HttpResponse::newHttpResponse(drogon::k401Unauthorized,
                                                  drogon::CT_APPLICATION_JSON);
        SimpleError err{.error = "Unauthorized: No valid token provided"};
        resp->setBody(glz::write_json(err).value_or(""));
        co_return resp;
      }
      std::string token = auth_header.substr(7);

      using traits = jwt::traits::open_source_parsers_jsoncpp;
      auto decoded = jwt::decode<traits>(token);

      // Verify signature and expiration 验证签名和过期
      auto verifier =
          jwt::verify<traits>()
              .allow_algorithm(jwt::algorithm::hs256{config::JWT_SECRET})
              .with_issuer("buyer-app");

      // Extract user ID from token and add to request attributes for later use 提取用户ID并添加到请求属性中以备后用
      verifier.verify(decoded);
      auto claim = decoded.get_payload_claim("user_id");
      auto payload_type = claim.get_type();
      std::string user_id;
      if (payload_type == jwt::json::type::integer ||
          payload_type == jwt::json::type::number)
        user_id = std::to_string(static_cast<int>(claim.as_number()));
      else if (payload_type == jwt::json::type::string)
        user_id = claim.as_string();
      else
        throw std::runtime_error("invalid type");

      // Pass user_id through request attributes 通过请求属性传递user_id
      req->getAttributes()->insert("current_user_id", user_id);

      // Token is valid, proceed to the next middleware/controller 令牌有效，继续进行下一个中间件/控制器
      auto resp = co_await next;
      co_return resp;
    } catch (const std::exception &e) {
      LOG_ERROR << "Auth error: " << e.what();
      auto resp = HttpResponse::newHttpResponse(drogon::k401Unauthorized,
                                                drogon::CT_APPLICATION_JSON);
      SimpleError err{.error = "Unauthorized: Invalid token"};
      resp->setBody(glz::write_json(err).value_or(""));
      co_return resp;
    }
  }
};
