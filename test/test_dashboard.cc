/**
 * @file test_dashboard.cc
 * @brief 仪表盘功能测试文件
 * @details 测试用户仪表盘数据获取、订单统计等功能。
 */

#include <drogon/HttpClient.h>           // 引入Drogon HTTP客户端头文件
#include <drogon/drogon_test.h>          // 引入Drogon测试框架头文件
#include <drogon/utils/Utilities.h>      // 引入Drogon工具函数头文件

#include <string>                        // 引入字符串类头文件

#include "helpers.hpp"                   // 引入测试辅助函数头文件

/**
 * @brief 仪表盘测试用例
 * @details 测试用户仪表盘数据获取、订单状态统计等功能。
 */
DROGON_TEST(DashboardTest) {
  auto db_client = drogon::app().getDbClient();

  helpers::cleanup_db();

  // Create test user for testing
  auto client = drogon::HttpClient::newHttpClient("http://127.0.0.1:5555");

  // Register user
  Json::Value register_json;
  register_json["username"] = "testdash";
  register_json["email"] = "testdash@example.com";
  register_json["password"] = "password123";
  auto register_req = drogon::HttpRequest::newHttpJsonRequest(register_json);
  register_req->setMethod(drogon::Post);
  register_req->setPath("/api/v1/auth/register");

  auto register_resp = client->sendRequest(register_req);
  REQUIRE(register_resp.second->getStatusCode() == drogon::k200OK);

  auto resp_json = register_resp.second->getJsonObject();
  CHECK((*resp_json)["status"].asString() == "success");
  CHECK((*resp_json)["token"].asString().length() > 0);

  std::string token = (*resp_json)["token"].asString();

  // Get user ID for test user
  auto user_result = db_client->execSqlSync(
      "SELECT id FROM users WHERE username = 'testdash'");
  CHECK(user_result.size() > 0);
  int user_id = user_result[0]["id"].as<int>();

  // Test 1: Create a new order and verify dashboard updates
  // Create an order
  Json::Value create_order_json;
  create_order_json["user_id"] = user_id;
  create_order_json["status"] = "pending";

  auto create_order_req =
      drogon::HttpRequest::newHttpJsonRequest(create_order_json);
  create_order_req->setMethod(drogon::Post);
  create_order_req->setPath("/api/v1/orders");
  create_order_req->addHeader("Authorization", "Bearer " + token);

  auto create_order_resp = client->sendRequest(create_order_req);
  CHECK(create_order_resp.second->getStatusCode() == drogon::k200OK);

  auto create_order_resp_json = create_order_resp.second->getJsonObject();
  int order_id = (*create_order_resp_json)["order_id"].asInt();

  // Test 2: Get dashboard data
  auto get_dashboard_req = drogon::HttpRequest::newHttpRequest();
  get_dashboard_req->setMethod(drogon::Get);
  get_dashboard_req->setPath("/api/v1/dashboard");
  get_dashboard_req->addHeader("Authorization", "Bearer " + token);

  auto get_dashboard_resp = client->sendRequest(get_dashboard_req);
  CHECK(get_dashboard_resp.second->getStatusCode() == drogon::k200OK);

  auto get_dashboard_resp_json = get_dashboard_resp.second->getJsonObject();

  // Check dashboard structure
  CHECK(get_dashboard_resp_json->isMember("orders"));

  // Check orders data in dashboard
  const auto& orders_data = (*get_dashboard_resp_json)["orders"];
  CHECK(orders_data.isObject());

  // Check that we have at least one status count
  bool has_status_counts = false;
  if (orders_data.isMember("pending") || orders_data.isMember("in_progress") ||
      orders_data.isMember("delivered") || orders_data.isMember("cancelled") ||
      orders_data.isMember("in_transit")) {
    has_status_counts = true;
  }
  CHECK(has_status_counts);

  // Get updated dashboard data
  auto get_updated_dashboard_req = drogon::HttpRequest::newHttpRequest();
  get_updated_dashboard_req->setMethod(drogon::Get);
  get_updated_dashboard_req->setPath("/api/v1/dashboard");
  get_updated_dashboard_req->addHeader("Authorization", "Bearer " + token);

  auto get_updated_dashboard_resp =
      client->sendRequest(get_updated_dashboard_req);
  CHECK(get_updated_dashboard_resp.second->getStatusCode() == drogon::k200OK);

  auto get_updated_dashboard_resp_json =
      get_updated_dashboard_resp.second->getJsonObject();
  const auto& updated_orders_data =
      (*get_updated_dashboard_resp_json)["orders"];

  // Check that pending count includes our new order
  if (updated_orders_data.isMember("pending")) {
    CHECK(updated_orders_data["pending"].asInt() >= 1);
  }

  // Test 3: Create another order with different status
  Json::Value create_order_json2;
  create_order_json2["user_id"] = user_id;
  create_order_json2["status"] = "in_progress";

  auto create_order_req2 =
      drogon::HttpRequest::newHttpJsonRequest(create_order_json2);
  create_order_req2->setMethod(drogon::Post);
  create_order_req2->setPath("/api/v1/orders");
  create_order_req2->addHeader("Authorization", "Bearer " + token);

  auto create_order_resp2 = client->sendRequest(create_order_req2);
  CHECK(create_order_resp2.second->getStatusCode() == drogon::k200OK);

  auto create_order_resp_json2 = create_order_resp2.second->getJsonObject();
  int order_id2 = (*create_order_resp_json2)["order_id"].asInt();

  // Get updated dashboard data again
  auto get_updated_dashboard_req2 = drogon::HttpRequest::newHttpRequest();
  get_updated_dashboard_req2->setMethod(drogon::Get);
  get_updated_dashboard_req2->setPath("/api/v1/dashboard");
  get_updated_dashboard_req2->addHeader("Authorization", "Bearer " + token);

  auto get_updated_dashboard_resp2 =
      client->sendRequest(get_updated_dashboard_req2);
  CHECK(get_updated_dashboard_resp2.second->getStatusCode() == drogon::k200OK);

  auto get_updated_dashboard_resp_json2 =
      get_updated_dashboard_resp2.second->getJsonObject();
  const auto& updated_orders_data2 =
      (*get_updated_dashboard_resp_json2)["orders"];

  // Check that in_progress count includes our new order
  if (updated_orders_data2.isMember("in_progress")) {
    CHECK(updated_orders_data2["in_progress"].asInt() >= 1);
  }

  helpers::cleanup_db();
}