/**
 * @file location.cc
 * @brief 位置控制器实现文件
 * @details 实现用户位置管理功能，包括位置记录、位置聚类、附近位置查找和DBSCAN重新聚类。
 */
#include "location.hpp"

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

#include <format>

#include "../utilities/conversion.hpp"
#include "../utilities/json_manipulation.hpp"
#include "common_req_n_resp.hpp"

using drogon::app;
using drogon::CT_APPLICATION_JSON;
using drogon::HttpRequestPtr;
using drogon::HttpResponse;
using drogon::HttpResponsePtr;
using drogon::k400BadRequest;
using drogon::k500InternalServerError;
using drogon::orm::DrogonDbException;

using api::v1::LocationController;

/**
 * @struct AddLocationRequest
 * @brief 添加位置请求数据结构
 * @details 包含用户位置的经纬度和GPS精度信息。
 */
struct AddLocationRequest {
  double latitude;                 // 纬度
  double longitude;                // 经度
  std::optional<double> gps_accuracy{100.0};  // GPS精度（米），默认100米
};

/**
 * @struct AddLocationResponse
 * @brief 添加位置响应数据结构
 * @details 返回位置保存状态和设备ID。
 */
struct AddLocationResponse {
  std::string status;              // 操作状态
  std::string device_id;           // 设备ID
};

/**
 * @struct ClusterData
 * @brief 位置聚类数据结构
 * @details 包含聚类ID、点数量和聚类中心点坐标。
 */
struct ClusterData {
  std::string id;                  // 聚类ID
  int point_count;                 // 聚类中的点数
  std::string centroid;            // ST_AsGeoJSON返回的中心点坐标字符串
};

/**
 * @struct LocationPointResponse
 * @brief 位置点响应数据结构
 * @details 包含位置点的经纬度、精度、用户ID、设备ID、聚类ID和距离信息。
 */
struct LocationPointResponse {
  double latitude;                 // 纬度
  double longitude;                // 经度
  double accuracy;                 // GPS精度
  std::string user_id;             // 用户ID
  std::string device_id;           // 设备ID
  std::string cluster_id;          // 聚类ID
  double distance;                 // 距离中心点的距离（米）
};

/**
 * @brief 添加用户位置信息
 * @details 保存用户的GPS位置信息到数据库，支持UPSERT操作（存在则更新），使用PostGIS存储地理坐标。
 * @param req HTTP请求指针，包含latitude、longitude和可选的gps_accuracy参数
 * @param callback 响应回调函数
 * @return Task协程对象
 */
drogon::Task<> LocationController::add_location(
    const HttpRequestPtr req,
    std::function<void(const HttpResponsePtr&)> callback) {
  try {
    AddLocationRequest add_req;
    auto parse_error = utilities::strict_read_json(add_req, req->getBody());

    if (parse_error || (add_req.latitude == 0.0 && add_req.longitude == 0.0)) {
      LOG_ERROR << "Invalid longitude or latitude, note that they are required";
      SimpleError error{.error =
                            "Invalid longitude or latitude, note that they are "
                            "required"};
      auto resp =
          HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    std::string user_id =
        req->getAttributes()->get<std::string>("current_user_id");

    std::string device_id =
        user_id + "_device";  // single device location stored per user_id

    auto client = app().getDbClient();
    co_await client->execSqlCoro(
        "INSERT INTO locations (user_id, latitude, longitude, accuracy, "
        "device_id, geom) "
        "VALUES ($1, $2, $3, $4, $5, ST_SetSRID(ST_MakePoint($3, $2), 4326)) "
        "ON CONFLICT (user_id) DO UPDATE "
        "SET latitude = EXCLUDED.latitude, longitude = EXCLUDED.longitude, "
        "accuracy = EXCLUDED.accuracy, geom = "
        "ST_SetSRID(ST_MakePoint(EXCLUDED.longitude, EXCLUDED.latitude), "
        "4326), device_id = EXCLUDED.device_id",
        std::stoi(user_id), add_req.latitude, add_req.longitude,
        add_req.gps_accuracy.value_or(100.0), device_id);

    AddLocationResponse response{.status = "success", .device_id = device_id};
    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(response).value_or(""));
    callback(resp);
  } catch (const DrogonDbException& e) {
    SimpleError error{
        .error = std::format("Failed to save location: {}", e.base().what())};
    LOG_ERROR << "Failed to create error: " << e.base().what();
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }
  co_return;
}

/**
 * @brief 获取位置聚类数据
 * @details 获取已聚类的位置数据，按偏移量分页，每页15条，包含聚类ID、点数和中心点坐标。
 * @param req HTTP请求指针，支持offset查询参数
 * @param callback 响应回调函数
 * @return Task协程对象
 */
// Paginated by offset (15 per page)
drogon::Task<> LocationController::get_clusters(
    const HttpRequestPtr req,
    std::function<void(const HttpResponsePtr&)> callback) {
  int64_t offset = 0;
  auto offset_str = req->getParameter("offset");
  if (!offset_str.empty()) {
    offset = convert::string_to_int(offset_str).value_or(0);
    if (offset < 0) offset = 0;
  }

  try {
    auto client = app().getDbClient();
    auto result = co_await client->execSqlCoro(
        "SELECT cluster_id, "
        "ST_AsGeoJSON(ST_Centroid(ST_Collect(geom::geometry))) as centroid, "
        "COUNT(*) as point_count FROM locations WHERE cluster_id IS NOT NULL "
        "GROUP BY cluster_id OFFSET $1 LIMIT 15",
        offset);

    std::vector<ClusterData> clusters_data;
    clusters_data.reserve(result.size());
    for (const auto& row : result) {
      clusters_data.push_back(
          ClusterData{.id = row["cluster_id"].as<std::string>(),
                      .point_count = row["point_count"].as<int>(),
                      .centroid = row["centroid"].as<std::string>()});
    }

    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(clusters_data).value_or(""));
    callback(resp);
  } catch (const DrogonDbException& e) {
    LOG_ERROR << "Database error getting clusters: " << e.base().what();
    SimpleError error{.error = e.base().what()};
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k500InternalServerError, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }
  co_return;
}

/**
 * @brief 查找附近位置点
 * @details 根据给定的经纬度和半径查找附近的用户位置点，按偏移量分页，每页15条。
 * @param req HTTP请求指针，支持lat、lon、radius和offset查询参数
 * @param callback 响应回调函数
 * @return Task协程对象
 */
// Paginated by offset (15 per page)
drogon::Task<> LocationController::find_nearby(
    const HttpRequestPtr req,
    std::function<void(const HttpResponsePtr&)> callback) {
  try {
    auto lat_str = req->getParameter("lat");
    auto lon_str = req->getParameter("lon");
    auto radius_str = req->getParameter("radius");

    if (radius_str.empty()) radius_str = "1000";  // Default 1km radius

    int64_t offset = 0;
    auto offset_str = req->getParameter("offset");
    if (!offset_str.empty()) {
      offset = convert::string_to_int(offset_str).value_or(0);
      if (offset < 0) offset = 0;
    }

    if (lat_str.empty() || lon_str.empty()) {
      SimpleError error{.error = "Missing lat/lon parameters"};
      auto resp =
          HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    double lat = convert::string_to_number<double>(lat_str).value_or(0.0);
    double lon = convert::string_to_number<double>(lon_str).value_or(0.0);
    double radius =
        convert::string_to_number<double>(radius_str).value_or(1000.0);

    auto client = app().getDbClient();
    auto result = co_await client->execSqlCoro(
        "SELECT user_id, latitude, longitude, accuracy, device_id, cluster_id, "
        "ST_Distance(geom, ST_SetSRID(ST_MakePoint($2, $1), 4326)::geography) "
        "as distance "
        "FROM locations "
        "WHERE ST_DWithin(geom, ST_SetSRID(ST_MakePoint($2, $1), "
        "4326)::geography, $3) "
        "ORDER BY distance OFFSET $4 LIMIT 15",
        lat, lon, radius, offset);

    std::vector<LocationPointResponse> nearby_points;
    nearby_points.reserve(result.size());
    for (const auto& row : result) {
      nearby_points.push_back(LocationPointResponse{
          .latitude = row["latitude"].as<double>(),
          .longitude = row["longitude"].as<double>(),
          .accuracy = row["accuracy"].as<double>(),
          .user_id = row["user_id"].as<std::string>(),
          .device_id = row["device_id"].as<std::string>(),
          .cluster_id = row["cluster_id"].as<std::string>(),
          .distance = row["distance"].as<double>()});
    }

    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(nearby_points).value_or(""));
    callback(resp);
  } catch (const DrogonDbException& e) {
    LOG_ERROR << "Database error finding nearby cluster : " << e.base().what();
    SimpleError error{.error = e.base().what()};
    auto resp = drogon::HttpResponse::newHttpResponse(
        drogon::k500InternalServerError, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }
  co_return;
}

/**
 * @brief 重新进行位置聚类
 * @details 使用PostGIS的DBSCAN算法对所有位置点进行重新聚类，支持自定义epsilon参数（聚类半径）。
 * @param req HTTP请求指针，支持epsilon查询参数（单位：米，默认1000米）
 * @param callback 响应回调函数
 * @return Task协程对象
 */
drogon::Task<> LocationController::recluster(
    const HttpRequestPtr req,
    std::function<void(const HttpResponsePtr&)> callback) {
  double epsilon = 1000.0;  // meters
  try {
    auto epsilon_str = req->getParameter("epsilon");
    if (!epsilon_str.empty()) {
      epsilon = convert::string_to_number<double>(epsilon_str).value_or(1000.0);
      if (epsilon < 0 || epsilon > 10'000'000) epsilon = 1000.0;
    }
  } catch (...) {
    epsilon = 1000.0;
  }

  try {
    // other Parameters for DBSCAN
    int min_points = 2;

    auto client = app().getDbClient();
    // CTE: Assign cluster numbers using PostGIS DBSCAN
    // and update the locations table with new cluster_ids
    auto result = co_await client->execSqlCoro(R"(
  WITH transformed AS (
    SELECT id, ST_Transform(geom::geometry, 3857) AS geom_3857
    FROM locations
  ),
  clustered AS (
    SELECT id,
           ST_ClusterDBSCAN(geom_3857, $1,$2) OVER () AS cluster_num
    FROM transformed
  )
  UPDATE locations loc
  SET cluster_id = CASE 
    WHEN c.cluster_num IS NULL THEN NULL
    ELSE 'cluster_' || c.cluster_num::text
  END
  FROM clustered c
  WHERE loc.id = c.id
  RETURNING loc.id, loc.cluster_id
)",
                                               epsilon, min_points);

    SimpleStatus response{.status = "clustering completed"};
    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(response).value_or(""));
    callback(resp);
  } catch (const DrogonDbException& e) {
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(
        glz::write_json(SimpleError{.error = e.base().what()}).value_or(""));
    callback(resp);
  }
  co_return;
}
