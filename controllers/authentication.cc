/**
 * @file authentication.cc
 * @brief 用户认证控制器实现文件
 * @details 实现用户登录、登出、令牌刷新和用户注册等认证功能，使用Argon2进行密码哈希，JWT进行令牌管理。
 */

#include "authentication.hpp"           // 引入认证控制器头文件

#include <argon2.h>                     // 引入Argon2密码哈希库
#include <drogon/HttpResponse.h>        // 引入Drogon HTTP响应头文件

// 确保了 jwt-cpp 使用 JsonCpp 后端，而不是默认的 picojson 解析器。
#ifndef JWT_DISABLE_PICOJSON
#define JWT_DISABLE_PICOJSON
#endif
#include <jwt-cpp/jwt.h>                // 引入JWT库核心头文件
#include <jwt-cpp/traits/open-source-parsers-jsoncpp/traits.h>  // 引入JWT JsonCpp后端支持

#include <chrono>                       // 引入时间库
#include <iomanip>                      // 引入IO流格式化头文件
#include <random>                       // 引入随机数生成器头文件
#include <span>                         // 引入跨度头文件
#include <sstream>                      // 引入字符串流头文件

#include "../config/config.hpp"         // 引入配置管理头文件
#include "../utilities/json_manipulation.hpp"  // 引入JSON操作工具头文件
#include "../utilities/validation.hpp"  // 引入数据验证工具头文件
#include "common_req_n_resp.hpp"        // 引入通用请求响应结构头文件

//32 字节的哈希输出
#define ARGON2_HASH_LEN 32              // Argon2哈希输出长度（32字节）
//16 字节的盐值
#define ARGON2_SALT_LEN 16              // Argon2盐值长度（16字节）

using drogon::app;                     // Drogon应用实例别名
using drogon::CT_APPLICATION_JSON;     // JSON内容类型别名
using drogon::HttpResponse;            // HTTP响应类别名
using drogon::k400BadRequest;          // 400错误码别名
using drogon::k401Unauthorized;        // 401错误码别名
using drogon::k500InternalServerError; // 500错误码别名
using drogon::orm::DrogonDbException;  // 数据库异常类别名

using api::v1::Authentication;         // 认证控制器类别名

/**
 * @brief 生成随机字符串
 * @param length 随机字符串长度
 * @return 指定长度的随机字符串
 * @details 使用Mersenne Twister随机数生成器生成包含数字和大小写字母的随机字符串。
 */
std::string generate_random_string(size_t length) {
  const std::string chars =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";    // 字符集：数字+大小写字母
  std::random_device rd;    // 硬件随机数生成器
  std::mt19937 generator(rd());    // Mersenne Twister随机数生成器
  std::uniform_int_distribution<> distribution(
      0, static_cast<int>(chars.size() - 1));    // 均匀分布，范围0到字符集大小-1

  std::string random_string;
  for (size_t i = 0; i < length; ++i) {
    random_string += chars[distribution(generator)];    // 随机选取字符并拼接
  }
  return random_string;
}

/**
 * @brief Base64编码函数
 * @param data 待编码的字节数据
 * @return Base64编码后的字符串
 * @details 实现标准Base64编码，将二进制数据转换为可打印的ASCII字符串。
 */
std::string base64_encode(std::span<const uint8_t> data) {
  static const char* encoding_table =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";    // Base64编码表
  size_t length = data.size();    // 获取数据长度
  std::string encoded;
  encoded.reserve(4 * ((length + 2) / 3));    // 预分配空间，Base64编码后长度约为原长度的4/3

  for (size_t i = 0; i < length; i += 3) {    // 每3个字节为一组进行编码
    uint32_t octet_a = i < length ? data[i] : 0;    // 第一个字节
    uint32_t octet_b = i + 1 < length ? data[i + 1] : 0;    // 第二个字节（不足补0）
    uint32_t octet_c = i + 2 < length ? data[i + 2] : 0;    // 第三个字节（不足补0）

    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;    // 合并为24位

    encoded.push_back(encoding_table[(triple >> 18) & 0x3F]);    // 取高6位
    encoded.push_back(encoding_table[(triple >> 12) & 0x3F]);    // 取下一组6位
    encoded.push_back(encoding_table[(triple >> 6) & 0x3F]);     // 取下一组6位
    encoded.push_back(encoding_table[triple & 0x3F]);            // 取低6位
  }

  // Add padding 添加填充字符
  size_t mod = length % 3;    // 计算余数
  if (mod) {
    encoded[encoded.size() - 1] = '=';    // 至少添加一个填充字符
    if (mod == 1) {
      encoded[encoded.size() - 2] = '=';    // 余数为1时添加两个填充字符
    }
  }

  return encoded;
}

/**
 * @brief 生成JWT令牌Token
 * @param user_id 用户ID
 * @param username 用户名
 * @return JWT令牌字符串
 * @details 使用HS256算法生成JWT令牌，包含签发者、签发时间、过期时间、用户ID和用户名等信息。
 */
std::string generate_jwt(int user_id, const std::string& username) {
  const std::string secret = config::JWT_SECRET;    // 获取JWT密钥

  const auto now = std::chrono::system_clock::now();    // 获取当前时间
  const auto exp = now + std::chrono::hours(1);    // 设置过期时间为1小时后

  using traits = jwt::traits::open_source_parsers_jsoncpp;    // 使用JsonCpp后端

  auto token =
      jwt::create<traits>()
          .set_issuer("buyer-app")  /* 签发者*/
          .set_issued_at(now)  /* 签发时间*/
          .set_expires_at(exp)  /* 过期时间*/
          .set_payload_claim("user_id",
                             jwt::basic_claim<traits>(std::to_string(user_id)))/*载荷*/
          .set_payload_claim("username", jwt::basic_claim<traits>(username))    // 添加用户名载荷
          .sign(jwt::algorithm::hs256{secret});    // 使用HS256算法签名

  return token;
}

/**
 * @brief 生成刷新令牌
 * @return 64位随机字符串作为刷新令牌
 * @details 调用generate_random_string生成64位的随机字符串作为刷新令牌。
 */
std::string generate_refresh_token() { return generate_random_string(64); }

/**
 * @brief 密码加密
 * @param password 明文密码
 * @return Argon2id哈希后的密码字符串（PHC格式）
 * @details 使用Argon2id算法对密码进行哈希处理，生成自包含的PHC格式哈希字符串。
 */
std::string hash_password_with_argon2(const std::string& password) {
  uint8_t salt[ARGON2_SALT_LEN]; // 16 字节的盐值
  std::random_device rd;//  随机数生成器
  std::mt19937 generator(rd());    // Mersenne Twister随机数生成器
  std::uniform_int_distribution<short> distribution(0, 255);// 统一分布，范围0-255

  for (size_t i = 0; i < ARGON2_SALT_LEN; ++i) {
    salt[i] = static_cast<uint8_t>(distribution(generator));    // 生成随机盐值
  }

  // Argon2 parameters Argon2参数配置
  uint32_t t_cost = 3;        // Number of iterations 迭代次数
  uint32_t m_cost = 1 << 16;  // 64 MiB memory cost 64 MiB 内存消耗
  uint32_t parallelism = 1;   // Number of threads 线程数

  size_t hash_size =
      argon2_encodedlen(t_cost, m_cost, parallelism, ARGON2_SALT_LEN,
                        ARGON2_HASH_LEN, Argon2_type::Argon2_id);    // 计算编码后哈希的长度
  std::string encoded_hash(hash_size, '\0');// 创建一个足够大的字符串以保存编码后的哈希值

  /*
   * 生成 PHC 格式的自包含编码哈希字符串 $argon2id$v=19$m=65536,t=3,p=1$<salt_b64>$<hash_b64>
   */
  int result = argon2id_hash_encoded(t_cost, m_cost, parallelism,
                                     password.c_str(), password.length(), salt,
                                     ARGON2_SALT_LEN, ARGON2_HASH_LEN,
                                     encoded_hash.data(), encoded_hash.size());    // 执行哈希计算

  if (result != ARGON2_OK) {    // 检查哈希计算是否成功
    LOG_ERROR << "Failed to hash password: " << argon2_error_message(result);
    throw std::runtime_error("Failed to hash password");    // 哈希失败抛出异常
  }

  return encoded_hash;
}

/**
 * @brief 密码验证
 * @param password 明文密码
 * @param hash 存储的密码哈希值
 * @return 如果密码匹配返回true，否则返回false
 * @details 使用Argon2id算法验证明文密码与存储的哈希值是否匹配。
 */
bool verify_password_with_argon2(const std::string& password,
                                 const std::string& hash) {
  // Format: $argon2id$v=19$m=65536,t=3,p=1$<salt>$<hash>
  // Parses and verifies parses encoded hash 解析并验证编码后的哈希
  int result =
      argon2id_verify(hash.c_str(), password.c_str(), password.length());    // 验证密码

  return result == ARGON2_OK;    // 返回验证结果
}

/**
 * @struct LoginCredentials
 * @brief 登录身份信息结构体
 * @details 存储用户登录时提供的用户名和密码。
 */
struct LoginCredentials {
  std::string username;       // 用户名
  std::string password;       // 密码
};

/**
 * @struct CredentialsResponse
 * @brief 登录响应结构体
 * @details 存储登录成功后返回的认证信息，包括状态、令牌、刷新令牌、用户ID和用户名。
 */
struct CredentialsResponse {
  std::string status;         // 状态
  std::string token;          // 令牌
  std::string refresh_token;  // 刷新令牌
  int user_id;                // 用户ID
  std::string username;       // 用户名
};

/**
 * @struct RefreshRequest
 * @brief 刷新令牌请求结构体
 * @details 存储用于刷新令牌的请求参数。
 */
struct RefreshRequest {
  std::string refresh_token;  // 刷新令牌
};

/**
 * @struct RegisterRequest
 * @brief 注册请求结构体
 * @details 存储用户注册时提供的用户名、邮箱和密码。
 */
struct RegisterRequest {
  std::string username;       // 用户名
  std::string email;          // 邮箱
  std::string password;       // 密码
};

/**
 * @brief 登录
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @details 验证用户登录凭证，验证成功后生成JWT令牌和刷新令牌，并存储会话信息。
 */
drogon::Task<> Authentication::login(
    const drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> callback) {
  auto body = req->getBody();// 获取请求体
  LoginCredentials creds;
  auto parse_error = utilities::strict_read_json(creds, body);    // 解析JSON请求体

  if (parse_error || creds.username.empty() || creds.password.empty()) {    // 验证请求参数
    LOG_WARN << "Wrong credentials schema";
    SimpleError ret{.error =
                        "Invalid request, requires valid username & password"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);    // 返回400错误
    resp->setBody(glz::write_json(ret).value_or(""));    // 设置响应体
    callback(resp);
    co_return;
  }

  // for debugging hashes
  // LOG_INFO << "Password hash:" << hash_password_with_argon2(password) <<
  // std::endl;

  auto db = app().getDbClient();    // 获取数据库客户端
  try {
    auto result = co_await db->execSqlCoro(
        "SELECT id, username, password_hash FROM users WHERE username = $1",
        creds.username);    // 查询用户信息

    if (result.empty()) {    // 用户不存在
      SimpleError ret{.error = "Invalid username/password"};
      auto resp =
          HttpResponse::newHttpResponse(k401Unauthorized, CT_APPLICATION_JSON);    // 返回401错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }
    const auto& row = result[0];
    int user_id = row["id"].as<int>();    // 获取用户ID
    std::string stored_hash = row["password_hash"].as<std::string>();    // 获取存储的密码哈希

    bool password_match =
        verify_password_with_argon2(creds.password, stored_hash);    // 验证密码

    if (!password_match) {    // 密码不匹配
      SimpleError ret{.error = "Invalid username/password"};
      auto resp =
          HttpResponse::newHttpResponse(k401Unauthorized, CT_APPLICATION_JSON);    // 返回401错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }

    std::string token = generate_jwt(user_id, creds.username);    // 生成JWT令牌

    std::string refresh_token = generate_refresh_token();    // 生成刷新令牌

    auto expiry = std::chrono::system_clock::now() +
                  std::chrono::hours(24 * 7);  // 1 week 设置过期时间为1周
    auto expiry_time = std::chrono::system_clock::to_time_t(expiry);    // 转换为时间戳

    try {
      // 存储会话,如果冲突则忽略
      co_await db->execSqlCoro(
          "INSERT INTO user_sessions (user_id, token, refresh_token, "
          "expires_at) VALUES ($1, $2, $3, to_timestamp($4)) "
          "ON CONFLICT (token) DO NOTHING",
          user_id, token, refresh_token, static_cast<double>(expiry_time));    // 存储会话

      CredentialsResponse response{.status = "success",
                                   .token = token,
                                   .refresh_token = refresh_token,
                                   .user_id = user_id,
                                   .username = creds.username};    // 构建响应

      auto resp =
          HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);    // 返回200成功
      resp->setBody(glz::write_json(response).value_or(""));    // 设置响应体
      callback(resp);
      co_return;
    } catch (const DrogonDbException& e) {    // 会话存储失败
      LOG_ERROR << "Failed to store session: " << e.base().what();
      SimpleError ret{.error = "An error occurred"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);    // 返回500错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }
  } catch (const DrogonDbException& e) {    // 数据库查询失败
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError ret{.error = "An error occurred"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);    // 返回500错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
    co_return;
  }

  co_return;
}

/**
 * @brief 登出
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @details 从数据库中删除用户会话，使令牌失效。
 */
drogon::Task<> Authentication::logout(
    const drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> callback) {
  const std::string& auth_header = req->getHeader("Authorization");    // 获取认证头

  if (!auth_header.empty() && auth_header.substr(0, 7) == "Bearer ") {    // 验证Bearer令牌格式
    std::string token = auth_header.substr(7);    // 提取令牌

    auto db = app().getDbClient();    // 获取数据库客户端
    try {
      co_await db->execSqlCoro("DELETE FROM user_sessions WHERE token = $1",
                               token);    // 删除会话

      SimpleStatus ret{.status = "success"};
      auto resp =
          HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);    // 返回200成功
      resp->setBody(glz::write_json(ret).value_or(""));    // 设置响应体
      callback(resp);
    } catch (const DrogonDbException& e) {    // 数据库操作失败
      LOG_ERROR << "Database error: " << e.base().what();
      SimpleError ret{.error = "An error occurred"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);    // 返回500错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
    }
  } else {
    SimpleStatus ret{.status = "success"};  // Still return success as the user
                                            // is effectively logged out
    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);    // 返回200成功
    resp->setBody(glz::write_json(ret).value_or(""));    // 设置响应体
    callback(resp);
  }

  co_return;
}

/**
 * @brief 刷新用户的认证令牌
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @details 使用刷新令牌获取新的JWT令牌，验证刷新令牌有效性和过期时间。
 */
drogon::Task<> Authentication::refresh(
    const drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> callback) {
  RefreshRequest refresh_req;
  auto parse_error = utilities::strict_read_json(refresh_req, req->getBody());    // 解析JSON请求体

  if (parse_error || refresh_req.refresh_token.empty()) {    // 验证刷新令牌
    SimpleError ret{.error = "Refresh token is required"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);    // 返回400错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
    co_return;
  }

  auto db = app().getDbClient();    // 获取数据库客户端
  try {
    auto result = co_await db->execSqlCoro(
        "SELECT user_id, expires_at FROM user_sessions WHERE refresh_token ="
        "$1",
        refresh_req.refresh_token);    // 查询会话信息

    if (result.empty()) {    // 刷新令牌无效
      SimpleError ret{.error = "Invalid refresh token"};
      auto resp =
          HttpResponse::newHttpResponse(k401Unauthorized, CT_APPLICATION_JSON);    // 返回401错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }
    const auto& row = result[0];
    int user_id = row["user_id"].as<int>();    // 获取用户ID

    auto expiry_str = row["expires_at"].as<std::string>();    // 获取过期时间字符串

    // Parse timestamp from PostgreSQL format (e.g., "2025-04-25 12:34:56")
    // 从PostgreSQL格式解析时间戳（例如"2025-04-25 12:34:56"）
    std::chrono::system_clock::time_point expiry_time_point; // 到期 时间
    std::istringstream ss(expiry_str);    // 创建字符串流
    // std::tm tm = {};
    // ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    ss >> std::chrono::parse("%Y-%m-%d %H:%M:%S", expiry_time_point);    // 解析时间字符串
    if (ss.fail()) {    // 解析失败
      LOG_ERROR << "Failed to parse expiry time: " << expiry_str;
      SimpleError ret{.error = "An error occurred"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);    // 返回500错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }

    // auto old_expiry_time = std::mktime(&tm); // to time_t to compare
    // auto now = std::chrono::system_clock::to_time_t(
    //    std::chrono::system_clock::now());

    auto now = std::chrono::system_clock::now();    // 获取当前时间

    if (expiry_time_point < now) {    // 刷新令牌已过期
      LOG_INFO << "Refresh token has expired";
      SimpleError ret{.error = "Refresh token has expired"};
      auto resp =
          HttpResponse::newHttpResponse(k401Unauthorized, CT_APPLICATION_JSON);    // 返回401错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }

    // Token is still valid, proceed with refresh
    // Get username for the token
    try {
      auto user_result = co_await db->execSqlCoro(
          "SELECT username FROM users WHERE id = $1", user_id);    // 查询用户名

      if (user_result.empty()) {    // 用户不存在
        SimpleError ret{.error = "Invalid refresh token"};
        auto resp = HttpResponse::newHttpResponse(k401Unauthorized,
                                                  CT_APPLICATION_JSON);    // 返回401错误
        resp->setBody(glz::write_json(ret).value_or(""));
        callback(resp);
        co_return;
      }
      std::string username = user_result[0]["username"].as<std::string>();    // 获取用户名

      std::string new_token = generate_jwt(user_id, username);    // 生成新的JWT令牌

      std::string new_refresh_token = generate_refresh_token();    // 生成新的刷新令牌

      auto expiry =
          std::chrono::system_clock::now() + std::chrono::hours(24 * 7);    // 设置过期时间为1周
      auto expiry_time = std::chrono::system_clock::to_time_t(expiry);    // 转换为时间戳

      try {
        co_await db->execSqlCoro(
            "UPDATE user_sessions SET token = $1, refresh_token = "
            "$2, expires_at = to_timestamp($3) WHERE refresh_token = $4",
            new_token, new_refresh_token, static_cast<double>(expiry_time),
            refresh_req.refresh_token);    // 更新会话信息

        CredentialsResponse response{.status = "success",
                                     .token = new_token,
                                     .refresh_token = new_refresh_token,
                                     .user_id = user_id,
                                     .username = username};    // 构建响应

        auto resp =
            HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);    // 返回200成功
        resp->setBody(glz::write_json(response).value_or(""));    // 设置响应体
        callback(resp);
      } catch (const DrogonDbException& e) {    // 更新会话失败
        LOG_ERROR << "Failed to update session: " << e.base().what();
        SimpleError ret{.error = "An error occurred"};
        auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                  CT_APPLICATION_JSON);    // 返回500错误
        resp->setBody(glz::write_json(ret).value_or(""));
        callback(resp);
      }

    } catch (const DrogonDbException& e) {    // 查询用户名失败
      LOG_ERROR << "Database error: " << e.base().what();
      SimpleError ret{.error = "An error occurred"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);    // 返回500错误
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
    }

  } catch (const DrogonDbException& e) {    // 查询会话失败
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError ret{.error = "An error occurred"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);    // 返回500错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief Register a new user 注册新用户
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @details 创建新用户账户，验证用户名和邮箱唯一性，对密码进行哈希处理，注册成功后自动登录。
 */
drogon::Task<> Authentication::register_user(
    const drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> callback) {
  RegisterRequest register_req;
  auto parse_error = utilities::strict_read_json(register_req, req->getBody());    // 解析JSON请求体

  if (parse_error || register_req.username.empty() ||
      !utilities::is_email_valid(register_req.email) ||
      register_req.password.empty()) {    // 验证请求参数
    SimpleError ret{.error = "Username, email, and password are required"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);    // 返回400错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
    co_return;
  }

  std::string password_hash;
  try {
    password_hash = hash_password_with_argon2(register_req.password);    // 对密码进行哈希
  } catch (const std::exception& e) {    // 哈希失败
    LOG_ERROR << "Failed to hash password: " << e.what();
    SimpleError ret{.error = "An error occurred during registration"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);    // 返回500错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
    co_return;
  }

  auto db = app().getDbClient();    // 获取数据库客户端

  try {
    auto result = co_await db->execSqlCoro(
        "SELECT id FROM users WHERE username = $1 OR email = $2",
        register_req.username, register_req.email);    // 检查用户名和邮箱是否已存在

    if (!result.empty()) {    // 用户名或邮箱已存在
      SimpleError ret{.error = "Username or email already exists"};
      auto resp = HttpResponse::newHttpResponse(drogon::k409Conflict,
                                                CT_APPLICATION_JSON);    // 返回409冲突
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    } else {
      try {
        auto insert_result = co_await db->execSqlCoro(
            "INSERT INTO users (username, email, password_hash) VALUES ($1, "
            "$2, $3) RETURNING id",
            register_req.username, register_req.email, password_hash);    // 插入新用户

        if (insert_result.empty()) {    // 插入失败
          SimpleError ret{.error = "Failed to create user"};
          auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                    CT_APPLICATION_JSON);    // 返回500错误
          resp->setBody(glz::write_json(ret).value_or(""));
          callback(resp);
          co_return;
        }
        int user_id = insert_result[0]["id"].as<int>();    // 获取新用户ID

        std::string token = generate_jwt(user_id, register_req.username);    // 生成JWT令牌

        std::string refresh_token = generate_refresh_token();    // 生成刷新令牌
        auto expiry =
            std::chrono::system_clock::now() + std::chrono::hours(24 * 7);    // 设置过期时间为1周
        auto expiry_time = std::chrono::system_clock::to_time_t(expiry);    // 转换为时间戳

        try {
          co_await db->execSqlCoro(
              "INSERT INTO user_sessions (user_id, token, refresh_token, "
              "expires_at) VALUES ($1, $2, $3, to_timestamp($4)) "
              "ON CONFLICT (token) DO NOTHING",
              user_id, token, refresh_token, static_cast<double>(expiry_time));    // 存储会话

          CredentialsResponse response{.status = "success",
                                       .token = token,
                                       .refresh_token = refresh_token,
                                       .user_id = user_id,
                                       .username = register_req.username};    // 构建响应

          auto resp =
              HttpResponse::newHttpResponse(drogon::k200OK,
                                            CT_APPLICATION_JSON);    // 返回200成功
          resp->setBody(glz::write_json(response).value_or(""));    // 设置响应体
          callback(resp);
        } catch (const DrogonDbException& e) {    // 会话存储失败
          LOG_ERROR << "Failed to store session: " << e.base().what();
          SimpleError ret{.error =
                              "Registration successful, but failed to log in"};
          auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                    CT_APPLICATION_JSON);    // 返回500错误
          resp->setBody(glz::write_json(ret).value_or(""));
          callback(resp);
        }

      } catch (const DrogonDbException& e) {    // 插入用户失败
        LOG_ERROR << "Database error: " << e.base().what();
        SimpleError ret{.error = "An error occurred during registration"};
        auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                  CT_APPLICATION_JSON);    // 返回500错误
        resp->setBody(glz::write_json(ret).value_or(""));
        callback(resp);
      }
    }
  } catch (const DrogonDbException& e) {    // 查询用户失败
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError ret{.error = "An error occurred during registration"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);    // 返回500错误
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
  }

  co_return;
}
