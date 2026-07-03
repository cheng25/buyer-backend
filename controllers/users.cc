/**
 * @file users.cc
 * @brief 用户控制器实现文件
 * @details 实现用户列表查询功能，支持分页查询，按用户名排序。
 */
#include "users.hpp"

#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <drogon/orm/Criteria.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Field.h>
#include <drogon/orm/Mapper.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/ResultIterator.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/SqlBinder.h>

#include "../utilities/conversion.hpp"
#include "../utilities/json_manipulation.hpp"
#include "common_req_n_resp.hpp"


using drogon::app;
using drogon::CT_APPLICATION_JSON;
using drogon::HttpResponse;

using api::v1::Users;

/**
 * @struct UserInfo
 * @brief 用户信息数据结构
 * @details 包含用户的基本信息，包括用户ID、用户名、邮箱和创建时间。
 */
struct UserInfo {
  int id;                           // 用户ID
  std::string username;             // 用户名
  std::string email;                // 邮箱地址
  std::string created_at;           // 创建时间
};

/**
 * @brief 获取用户列表
 * @details 查询用户列表，支持分页参数（page和pageSize），按用户名排序。
 * @param req HTTP请求指针
 * @param callback HTTP响应回调函数
 * @return Task<> 异步任务
 */
drogon::Task<> Users::get_users(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> callback) {
  auto db = app().getDbClient();

  std::size_t page = 1;             // 当前页码，默认为1
  std::size_t pageSize = 20;        // 每页大小，默认为20

  // 安全解析分页参数
  if (!req->getParameter("page").empty()) {
    page = std::max(
        1, convert::string_to_int(req->getParameter("page")).value_or(1));
  }
  if (!req->getParameter("pageSize").empty()) {
    pageSize = std::max(
        1, std::min(100, convert::string_to_int(req->getParameter("pageSize"))
                             .value_or(20)));
  }
  std::size_t offset = (page - 1) * pageSize;

  try {
    auto result = co_await db->execSqlCoro(
        "SELECT id, username, email, created_at FROM users ORDER BY username "
        "LIMIT $1 OFFSET $2",
        pageSize, offset);

    std::vector<UserInfo> users_data;
    users_data.reserve(result.size());
    for (const auto& row : result) {
      users_data.emplace_back(
          UserInfo{.id = row["id"].as<int>(),
                   .username = row["username"].as<std::string>(),
                   .email = row["email"].as<std::string>(),
                   .created_at = row["created_at"].as<std::string>()});
    }

    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(users_data).value_or(""));
    callback(resp);
  } catch (const drogon::orm::DrogonDbException& e) {
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError error{.error = "Database error"};
    auto resp = HttpResponse::newHttpResponse(drogon::k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}
