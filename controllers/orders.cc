/**
 * @file orders.cc
 * @brief 订单控制器实现文件
 * @details 实现订单的查询和创建功能，支持分页查询。
 */
#include "orders.hpp"

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
using drogon::HttpRequestPtr;
using drogon::HttpResponse;
using drogon::HttpResponsePtr;
using drogon::k500InternalServerError;
using drogon::Task;
using drogon::orm::DrogonDbException;

using api::v1::Orders;

/**
 * @struct OrderInfo
 * @brief 订单信息数据结构
 * @details 包含订单的基本信息，包括订单ID、用户ID、状态和创建时间。
 */
struct OrderInfo {
  int id;                           // 订单ID
  int user_id;                      // 用户ID
  std::string status;               // 订单状态
  std::string created_at;           // 创建时间
};

/**
 * @struct CreateOrderRequest
 * @brief 创建订单请求数据结构
 * @details 包含创建订单所需的用户ID和订单状态。
 */
struct CreateOrderRequest {
  int user_id;                      // 用户ID
  std::string status;               // 订单状态
};

/**
 * @struct CreateOrderResponse
 * @brief 创建订单响应数据结构
 * @details 返回创建状态和新创建的订单ID。
 */
struct CreateOrderResponse {
  std::string status;               // 创建状态
  int order_id;                     // 新创建的订单ID
};

/**
 * @brief 获取订单列表
 * @details 查询订单列表，支持分页参数（page和pageSize），按订单ID降序排列。
 * @param req HTTP请求指针
 * @param callback HTTP响应回调函数
 * @return Task<> 异步任务
 */
Task<> Orders::get_orders(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback) {
  auto db = app().getDbClient();

  // 分页参数
  std::size_t page = 1;             // 当前页码，默认为1
  std::size_t pageSize = 20;        // 每页大小，默认为20

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
        "SELECT * FROM orders ORDER BY id DESC LIMIT $1 OFFSET $2", pageSize,
        offset);

    std::vector<OrderInfo> orders_data;
    orders_data.reserve(result.size());
    for (const auto& row : result) {
      orders_data.emplace_back(
          OrderInfo{.id = row["id"].as<int>(),
                    .user_id = row["user_id"].as<int>(),
                    .status = row["status"].as<std::string>(),
                    .created_at = row["created_at"].as<std::string>()});
    }

    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(orders_data).value_or(""));
    callback(resp);
  } catch (const DrogonDbException& e) {
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError error{.error = "Database error"};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 创建新订单
 * @details 根据请求参数创建新订单，验证用户ID和订单状态不能为空。
 * @param req HTTP请求指针
 * @param callback HTTP响应回调函数
 * @return Task<> 异步任务
 */
Task<> Orders::create_order(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback) {
  auto db = app().getDbClient();

  // 解析创建订单请求
  CreateOrderRequest create_req;
  auto parse_error = utilities::strict_read_json(create_req, req->getBody());

  if (parse_error || create_req.user_id <= 0 || create_req.status.empty()) {
    SimpleError error{.error = "Invalid JSON or missing fields"};
    auto resp = HttpResponse::newHttpResponse(drogon::k400BadRequest,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
    co_return;
  }

  try {
    auto result = co_await db->execSqlCoro(
        "INSERT INTO orders (user_id, status) VALUES ($1, $2) RETURNING id",
        create_req.user_id, create_req.status);

    if (result.empty()) {
      SimpleStatus ret{.status = "failed"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(ret).value_or(""));
      callback(resp);
      co_return;
    }
    CreateOrderResponse response{.status = "success",
                                 .order_id = result[0]["id"].as<int>()};

    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(response).value_or(""));
    callback(resp);
  } catch (const DrogonDbException& e) {
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError error{.error = e.base().what()};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}
