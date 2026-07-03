/**
 * @file search.cc
 * @brief 搜索控制器实现文件
 * @details 实现全站搜索功能，支持在订单和帖子中进行模糊搜索，支持分页查询。
 */
#include "search.hpp"

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
using drogon::HttpResponsePtr;
using drogon::orm::DrogonDbException;

using api::v1::Search;

/**
 * @struct SearchResultItem
 * @brief 搜索结果项数据结构
 * @details 包含搜索结果的基本信息，包括结果ID、类型和详细描述。
 */
struct SearchResultItem {
  int id;                           // 结果ID
  std::string type;                 // 结果类型（如 Order、Post）
  std::string details;              // 结果详细描述
};

/**
 * @brief 执行全站搜索
 * @details 在订单和帖子中进行模糊搜索，支持分页查询。搜索条件使用ILIKE进行大小写不敏感匹配。
 * 如果帖子搜索失败，仍然返回订单搜索结果。
 * @param req HTTP请求指针，包含查询参数（query、page、pageSize）
 * @param callback HTTP响应回调函数
 * @return Task<> 异步任务
 */
drogon::Task<> Search::search(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> callback) {
  auto query = req->getParameter("query");  // 获取搜索关键词
  std::size_t page = 1;                     // 当前页码，默认为1
  std::size_t pageSize = 20;                // 每页大小，默认为20

  // 解析页码参数
  if (req->getParameter("page").empty() == false) {
    page = std::max(
        1, convert::string_to_int(req->getParameter("page")).value_or(1));
  }

  // 解析每页大小参数，限制范围为1-100
  if (req->getParameter("pageSize").empty() == false) {
    pageSize = std::max(
        1, std::min(100, convert::string_to_int(req->getParameter("pageSize"))
                             .value_or(20)));
  }

  std::size_t offset = (page - 1) * pageSize;  // 计算偏移量

  LOG_DEBUG << "Search query received: '" << query << "', page: " << page
            << ", pageSize: " << pageSize;

  // 空查询返回空结果
  if (query.empty()) {
    std::vector<SearchResultItem> empty_results = {};
    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(empty_results).value_or(""));
    callback(resp);
    co_return;
  }

  auto db = app().getDbClient();
  try {
    auto orders_result = co_await db->execSqlCoro(
        "SELECT * FROM orders WHERE CAST(id AS TEXT) ILIKE $1 OR status ILIKE "
        "$1 ORDER BY id DESC LIMIT $2 OFFSET $3",
        "%" + query + "%", pageSize, offset);

    std::vector<SearchResultItem> search_results_data;
    search_results_data.reserve(orders_result.size());

    LOG_DEBUG << "Found " << orders_result.size()
              << " matching orders for query: '" << query << "'";

    for (const auto& row : orders_result) {
      search_results_data.emplace_back(
          SearchResultItem{.id = row["id"].as<int>(),
                           .type = "Order",
                           .details = "Order #" + row["id"].as<std::string>() +
                                      " - " + row["status"].as<std::string>()});
    }

    try {
      auto posts_result = co_await db->execSqlCoro(
          "SELECT * FROM posts WHERE content ILIKE $1 ORDER BY id DESC LIMIT "
          "$2 OFFSET $3",
          "%" + query + "%", pageSize, offset);

      LOG_DEBUG << "Found " << posts_result.size()
                << " matching posts for query: '" << query << "'";
      search_results_data.reserve(orders_result.size() + posts_result.size());
      for (const auto& row : posts_result) {
        std::string content = row["content"].as<std::string>();
        search_results_data.emplace_back(SearchResultItem{
            .id = row["id"].as<int>(),
            .type = "Post",
            .details = "Post: " + (content.length() > 50
                                       ? content.substr(0, 50) + "..."
                                       : content)});
      }

      auto resp =
          HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(search_results_data).value_or(""));
      callback(resp);
    } catch (const DrogonDbException& e) {
      LOG_ERROR << "Database error searching posts: " << e.base().what();
      // Still return the order results even if post search fails
      auto resp =
          HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(search_results_data).value_or(""));
      callback(resp);
    }
  } catch (const DrogonDbException& e) {
    LOG_ERROR << "Database error searching orders: " << e.base().what();
    SimpleError error{.error = "Database error"};
    auto resp = HttpResponse::newHttpResponse(drogon::k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}
