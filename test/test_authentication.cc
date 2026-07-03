/**
 * @file test_authentication.cc
 * @brief 用户认证功能测试文件
 * @details 测试用户注册、登录、令牌刷新和登出等认证相关功能。
 */

#include <drogon/HttpClient.h>           // 引入Drogon HTTP客户端头文件
#include <drogon/drogon_test.h>          // 引入Drogon测试框架头文件
#include <drogon/utils/Utilities.h>      // 引入Drogon工具函数头文件

#include <string>                        // 引入字符串类头文件

#include "helpers.hpp"                   // 引入测试辅助函数头文件

/**
 * @brief 用户认证测试用例
 * @details 测试用户注册、登录、令牌刷新和登出功能。
 */
DROGON_TEST(AuthenticationTest) {
  auto db_client = drogon::app().getDbClient();    // 获取数据库客户端

  helpers::cleanup_db();    // 清理数据库测试数据

  // Test registration 测试用户注册
  auto client = drogon::HttpClient::newHttpClient("http://127.0.0.1:5555");    // 创建HTTP客户端
  Json::Value request_json;
  request_json["username"] = "testuser";           // 设置用户名
  request_json["email"] = "testuser@example.com";  // 设置邮箱
  request_json["password"] = "password123";        // 设置密码
  auto req = drogon::HttpRequest::newHttpJsonRequest(request_json);    // 创建JSON请求
  req->setMethod(drogon::Post);                    // 设置请求方法为POST
  req->setPath("/api/v1/auth/register");           // 设置请求路径

  auto resp = client->sendRequest(req);                        // 发送请求
  REQUIRE(resp.second->getStatusCode() == drogon::k200OK);     // 验证响应状态码

  auto json = resp.second->getJsonObject();                    // 获取响应JSON

  REQUIRE((*json)["token"].asString().length() > 0);           // 验证token不为空
  REQUIRE((*json)["refresh_token"].asString().length() > 0);   // 验证refresh_token不为空

  std::string token = (*json)["token"].asString();             // 提取token
  std::string refresh_token = (*json)["refresh_token"].asString();   // 提取refresh_token

  // logout first since registering logs you in
  // 先登出，因为注册后会自动登录
  req = drogon::HttpRequest::newHttpRequest();    // 创建新请求
  req->setMethod(drogon::Post);                    // 设置请求方法为POST
  req->setPath("/api/v1/auth/logout");             // 设置请求路径
  req->addHeader("Authorization", "Bearer " + token);    // 添加认证头

  resp = client->sendRequest(req);                        // 发送请求
  REQUIRE(resp.second->getStatusCode() == drogon::k200OK);     // 验证响应状态码
  json = resp.second->getJsonObject();                        // 获取响应JSON

  // Test login 测试用户登录
  Json::Value login_json;
  login_json["username"] = "testuser";           // 设置用户名
  login_json["password"] = "password123";        // 设置密码
  req = drogon::HttpRequest::newHttpJsonRequest(login_json);    // 创建JSON请求
  req->setMethod(drogon::Post);                    // 设置请求方法为POST
  req->setPath("/api/v1/auth/login");              // 设置请求路径

  resp = client->sendRequest(req);                        // 发送请求
  REQUIRE(resp.second->getStatusCode() == drogon::k200OK);     // 验证响应状态码
  json = resp.second->getJsonObject();                        // 获取响应JSON

  REQUIRE((*json)["token"].asString().length() > 0);           // 验证token不为空
  REQUIRE((*json)["refresh_token"].asString().length() > 0);   // 验证refresh_token不为空

  token = (*json)["token"].asString();             // 提取token
  refresh_token = (*json)["refresh_token"].asString();   // 提取refresh_token

  // Test refresh token 测试令牌刷新
  Json::Value refresh_json;
  refresh_json["refresh_token"] = refresh_token;   // 设置refresh_token
  req = drogon::HttpRequest::newHttpJsonRequest(refresh_json);    // 创建JSON请求
  req->setMethod(drogon::Post);                    // 设置请求方法为POST
  req->setPath("/api/v1/auth/refresh");            // 设置请求路径
  req->addHeader("Authorization", "Bearer " + token);    // 添加认证头

  resp = client->sendRequest(req);                        // 发送请求
  REQUIRE(resp.second->getStatusCode() == drogon::k200OK);     // 验证响应状态码
  json = resp.second->getJsonObject();                        // 获取响应JSON

  REQUIRE((*json)["token"].asString().length() > 0);           // 验证新token不为空

  // Test logout 测试用户登出
  req = drogon::HttpRequest::newHttpRequest();    // 创建新请求
  req->setMethod(drogon::Post);                    // 设置请求方法为POST
  req->setPath("/api/v1/auth/logout");             // 设置请求路径
  req->addHeader("Authorization", "Bearer " + token);    // 添加认证头

  resp = client->sendRequest(req);                        // 发送请求
  REQUIRE(resp.second->getStatusCode() == drogon::k200OK);     // 验证响应状态码
  json = resp.second->getJsonObject();                        // 获取响应JSON

  helpers::cleanup_db();    // 清理数据库测试数据
}
