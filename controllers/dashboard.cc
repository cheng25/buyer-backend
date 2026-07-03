/**
 * @file dashboard.cc
 * @brief 仪表盘控制器实现文件
 * @details 实现仪表盘数据获取功能，包括订单统计等概览信息。
 */
#include "dashboard.hpp"

#include <drogon/HttpResponse.h>

#include "../utilities/conversion.hpp"
#include "../utilities/json_manipulation.hpp"
#include "common_req_n_resp.hpp"

using drogon::app;
using drogon::CT_APPLICATION_JSON;
using drogon::HttpResponse;

#include <algorithm>
#include <string>
#include <unordered_map>

using api::v1::Dashboard;

/**
 * @struct OrdersData
 * @brief 订单统计数据结构
 * @details 包含按状态分组的订单数量统计。
 */
struct OrdersData {
  std::unordered_map<std::string, int> orders;  // 订单状态到数量的映射
};

/**
 * @brief 获取仪表盘数据
 * @details 获取订单按状态分组的统计数据，支持分页查询。
 * @param req HTTP请求指针，支持page和pageSize查询参数
 * @param callback 响应回调函数
 * @return Task协程对象
 */
drogon::Task<> Dashboard::get_dashboard_data(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> callback) {
  auto db = app().getDbClient();

  // Pagination parameters
  std::size_t page = 1;
  std::size_t pageSize = 20;

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
        "SELECT status, COUNT(*) as count FROM orders GROUP BY status ORDER BY "
        "status LIMIT $1 OFFSET $2",
        pageSize, offset);

    OrdersData orders_data;
    for (const auto& row : result) {
      orders_data.orders.emplace(row["status"].as<std::string>(),
                                 row["count"].as<int>());
    }

    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(orders_data).value_or(""));
    callback(resp);
  } catch (const drogon::orm::DrogonDbException& e) {
    LOG_ERROR << "Database error: " << e.base().what();
    SimpleError ret{.error = "Database error"};
    auto resp = HttpResponse::newHttpResponse(drogon::k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(ret).value_or(""));
    callback(resp);
  }

  co_return;
}
