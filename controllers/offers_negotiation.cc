/**
 * @file offers_negotiation.cc
 * @brief 报价协商控制器实现文件
 * @details 实现报价协商功能，包括价格协商、产品证明请求与提交、托管交易创建等功能。
 */
#include <drogon/HttpController.h>
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
#include <drogon/utils/Utilities.h>

#include <format>
#include <string>
#include <vector>

#include "../services/service_manager.hpp"
#include "../utilities/conversion.hpp"
#include "../utilities/json_manipulation.hpp"
#include "common_req_n_resp.hpp"
#include "offers.hpp"

using drogon::app;
using drogon::CT_APPLICATION_JSON;
using drogon::HttpResponse;
using drogon::HttpResponsePtr;
using drogon::k400BadRequest;
using drogon::k403Forbidden;
using drogon::k404NotFound;
using drogon::k500InternalServerError;
using drogon::Task;
using drogon::orm::DrogonDbException;
using drogon::orm::Transaction;

using api::v1::Offers;

/**
 * @struct NegotiateOfferRequest
 * @brief 协商报价请求数据结构
 * @details 包含提议价格和可选消息。
 */
struct NegotiateOfferRequest {
  double price;                     // 提议价格
  std::optional<std::string> message;  // 协商消息（可选）
};

/**
 * @struct NegotiateOfferResponse
 * @brief 协商报价响应数据结构
 * @details 返回协商状态、消息、协商ID和对话ID。
 */
struct NegotiateOfferResponse {
  std::string status;               // 操作状态
  std::string message;              // 响应消息
  int negotiation_id;               // 协商记录ID
  std::string conversation_id;      // 对话ID
};

/**
 * @struct NegotiationInfo
 * @brief 协商信息数据结构
 * @details 包含协商记录的完整信息。
 */
struct NegotiationInfo {
  int id;                           // 协商记录ID
  int offer_id;                     // 报价ID
  int user_id;                      // 用户ID
  std::string username;             // 用户名
  double proposed_price;            // 提议价格
  std::string status;               // 协商状态
  std::optional<std::string> message;  // 协商消息（可选）
  std::string created_at;           // 创建时间
  std::string updated_at;           // 更新时间
};

/**
 * @struct GetNegotiationsResponse
 * @brief 获取协商列表响应数据结构
 * @details 返回协商状态和协商列表。
 */
struct GetNegotiationsResponse {
  std::string status;               // 操作状态
  std::vector<NegotiationInfo> negotiations;  // 协商记录列表
};

/**
 * @struct MessageMetadata
 * @brief 消息元数据数据结构
 * @details 包含报价ID和报价状态信息，用于消息上下文。
 */
struct MessageMetadata {
  std::string offer_id;             // 报价ID
  std::string offer_status{"pending"};  // 报价状态，默认pending
};

/**
 * @brief 创建或获取两个用户之间的对话
 * @details 检查两个用户之间是否已存在对话，若存在则返回现有对话ID，否则创建新对话。
 * @param user1_id 用户1的ID
 * @param user2_id 用户2的ID
 * @param offer_id 报价ID，用于生成对话名称
 * @return Task协程对象，返回对话ID字符串
 */
// Helper function to create a conversation between two users
Task<std::string> create_or_get_conversation(std::string user1_id,
                                             std::string user2_id,
                                             std::string offer_id) {
  auto db = app().getDbClient();

  // Check if a conversation already exists between these users
  try {
    auto result = co_await db->execSqlCoro(
        "SELECT c.id FROM conversations c "
        "JOIN conversation_participants cp1 ON c.id = cp1.conversation_id "
        "JOIN conversation_participants cp2 ON c.id = cp2.conversation_id "
        "WHERE cp1.user_id = $1 AND cp2.user_id = $2 "
        "LIMIT 1",
        convert::string_to_int(user1_id).value(),
        convert::string_to_int(user2_id).value());

    if (!result.empty()) {
      co_return std::to_string(result[0]["id"].as<int>());
    }

    // Create a new conversation
    auto conv_result = co_await db->execSqlCoro(
        "INSERT INTO conversations (name) VALUES ($1) RETURNING id",
        "Offer #" + offer_id + " Negotiation");

    if (conv_result.empty()) {
      co_return "";
    }

    int conversation_id = conv_result[0]["id"].as<int>();

    // Add participants
    co_await db->execSqlCoro(
        "INSERT INTO conversation_participants (conversation_id, user_id) "
        "VALUES "
        "($1, $2), ($1, $3)",
        conversation_id, convert::string_to_int(user1_id).value(),
        convert::string_to_int(user2_id).value());

    co_return std::to_string(conversation_id);
  } catch (const DrogonDbException& e) {
    LOG_ERROR << "Database error in create_or_get_conversation: "
              << e.base().what();
    co_return "";
  }
}

/**
 * @brief 在事务中创建或获取两个用户之间的对话
 * @details 在数据库事务中执行，确保对话创建与其他操作的原子性。
 * @param transaction 数据库事务指针
 * @param user1_id 用户1的ID
 * @param user2_id 用户2的ID
 * @param offer_id 报价ID，用于生成对话名称
 * @return Task协程对象，返回对话ID字符串
 */
// Create/get conversations during a transaction
Task<std::string> create_or_get_conversation_transaction(
    const std::shared_ptr<Transaction>& transaction, std::string user1_id,
    std::string user2_id, std::string offer_id) {
  try {
    auto result = co_await transaction->execSqlCoro(
        "SELECT c.id FROM conversations c "
        "JOIN conversation_participants cp1 ON c.id = cp1.conversation_id "
        "JOIN conversation_participants cp2 ON c.id = cp2.conversation_id "
        "WHERE cp1.user_id = $1 AND cp2.user_id = $2 "
        "LIMIT 1",
        convert::string_to_int(user1_id).value(),
        convert::string_to_int(user2_id).value());

    if (!result.empty()) {
      co_return std::to_string(result[0]["id"].as<int>());
    }

    LOG_INFO << "No existing conversation found, creating new one";

    auto conv_result = co_await transaction->execSqlCoro(
        "INSERT INTO conversations (name) VALUES ($1) RETURNING id",
        "Offer #" + offer_id + " Negotiation");

    if (conv_result.empty()) {
      LOG_ERROR << "Failed to create new conversation";
      co_return "";
    }

    int conversation_id = conv_result[0]["id"].as<int>();

    co_await transaction->execSqlCoro(
        "INSERT INTO conversation_participants (conversation_id, user_id) "
        "VALUES "
        "($1, $2), ($1, $3)",
        conversation_id, convert::string_to_int(user1_id).value(),
        convert::string_to_int(user2_id).value());

    co_return std::to_string(conversation_id);
  } catch (const DrogonDbException& e) {
    LOG_ERROR << "Database error in create_or_get_conversation: "
              << e.base().what();
    co_return "";
  }
}

// Dead code: Helper function to add a negotiation message to a conversation
void add_negotiation_message(
    std::shared_ptr<Transaction> transaction, std::string conversation_id,
    std::string sender_id, double proposed_price, std::string message,
    int negotiation_id, std::string offer_id,
    std::function<void(const HttpResponsePtr&)> callback) {
  std::string negotiation_message =
      std::format("Proposed new price: ${:.2f}", proposed_price);

  if (!message.empty()) {
    negotiation_message += "\nMessage: " + message;
  }

  MessageMetadata metadata{.offer_id = offer_id, .offer_status = "pending"};

  transaction->execSqlAsync(
      "INSERT INTO messages "
      "(conversation_id, sender_id, content, context_type, context_id, "
      "metadata) "
      "VALUES ($1, $2, $3, 'negotiation', $4, $5::jsonb)",
      [=](const drogon::orm::Result&) {
        LOG_INFO << "Message added successfully, completing transaction";

        NegotiateOfferResponse response{.status = "success",
                                        .message = "Negotiation started",
                                        .negotiation_id = negotiation_id,
                                        .conversation_id = conversation_id};
        auto resp =
            HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
        resp->setBody(glz::write_json(response).value_or(""));
        callback(resp);
      },
      [=](const DrogonDbException& e) {
        LOG_ERROR << "Database error adding message: " << e.base().what();
        SimpleError error{.error = "Database error: " +
                                   std::string(e.base().what())};
        auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                  CT_APPLICATION_JSON);
        resp->setBody(glz::write_json(error).value_or(""));
        callback(resp);
        transaction->rollback();
      },
      convert::string_to_int(conversation_id).value(),
      convert::string_to_int(sender_id).value(), negotiation_message,
      negotiation_id, glz::write_json(metadata).value_or(""));
}

/**
 * @brief 协商报价
 * @details 用户提出新的价格进行协商，更新报价状态为进行中，创建协商记录，并在对话中添加协商消息。
 * @param req HTTP请求指针，包含price和可选的message参数
 * @param callback 响应回调函数
 * @param id 报价ID字符串
 * @return Task协程对象
 */
// Negotiate an offer
Task<> Offers::negotiate_offer(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
    std::string id) {
  std::string current_user_id =
      req->getAttributes()->get<std::string>("current_user_id");
  auto db = app().getDbClient();

  NegotiateOfferRequest negotiate_req;
  auto parse_error = utilities::strict_read_json(negotiate_req, req->getBody());

  if (parse_error || negotiate_req.price < 0.0) {
    SimpleError error{.error = "Invalid request"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
    co_return;
  }

  double proposed_price = negotiate_req.price;
  std::string message = negotiate_req.message.value_or("");

  LOG_INFO << "Starting negotiate_offer for offer ID: " << id;

  try {
    auto transaction = co_await db->newTransactionCoro();

    try {
      // First, check if the offer exists and get the other user's ID
      auto result = co_await transaction->execSqlCoro(
          "SELECT o.*, p.user_id AS post_user_id "
          "FROM offers o "
          "JOIN posts p ON o.post_id = p.id "
          "WHERE o.id = $1",
          convert::string_to_int(id).value());

      if (result.empty()) {
        SimpleError error{.error = "Offer not found"};
        auto resp =
            HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);
        resp->setBody(glz::write_json(error).value_or(""));
        callback(resp);
        transaction->rollback();
        co_return;
      }

      // Check if user is authorized (must be either offer creator or post
      // owner)
      int offer_user_id = result[0]["user_id"].as<int>();
      int post_user_id = result[0]["post_user_id"].as<int>();

      LOG_INFO << "offer_user_id: " << offer_user_id;
      LOG_INFO << "current_user_id: " << current_user_id;
      LOG_INFO << "post_user_id: " << post_user_id;

      if (convert::string_to_int(current_user_id).value() != offer_user_id &&
          convert::string_to_int(current_user_id).value() != post_user_id) {
        SimpleError error{.error = "Unauthorized"};
        auto resp =
            HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);
        resp->setBody(glz::write_json(error).value_or(""));
        callback(resp);
        transaction->rollback();
        co_return;
      }

      co_await transaction->execSqlCoro(
          "UPDATE offers SET negotiation_status = 'in_progress' WHERE id = $1",
          convert::string_to_int(id).value());

      auto neg_result = co_await transaction->execSqlCoro(
          "INSERT INTO price_negotiations "
          "(offer_id, user_id, proposed_price, message) "
          "VALUES ($1, $2, $3, $4) "
          "RETURNING id, created_at",
          convert::string_to_int(id).value(),
          convert::string_to_int(current_user_id).value(), proposed_price,
          message);

      if (neg_result.empty()) {
        SimpleError error{.error = "Failed to create negotiation"};
        auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                  CT_APPLICATION_JSON);
        resp->setBody(glz::write_json(error).value_or(""));
        callback(resp);
        transaction->rollback();
        co_return;
      }

      int negotiation_id = neg_result[0]["id"].as<int>();

      // Determine the other user ID
      std::string other_user_id = std::to_string(
          convert::string_to_int(current_user_id).value() == offer_user_id
              ? post_user_id
              : offer_user_id);

      std::string conversation_id =
          co_await create_or_get_conversation_transaction(
              transaction, current_user_id, other_user_id, id);

      if (conversation_id.empty()) {
        SimpleError error{.error = "Failed to create or get conversation"};
        auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                  CT_APPLICATION_JSON);
        resp->setBody(glz::write_json(error).value_or(""));
        callback(resp);
        transaction->rollback();
        co_return;
      }

      std::string negotiation_message =
          std::format("Proposed new price: ${:.2f}", proposed_price);

      if (!message.empty()) {
        negotiation_message += "\nMessage: " + message;
      }

      LOG_INFO << "Negotiation message: " << negotiation_message;

      MessageMetadata metadata{.offer_id = id, .offer_status = "pending"};

      co_await transaction->execSqlCoro(
          "INSERT INTO messages "
          "(conversation_id, sender_id, content, context_type, context_id, "
          "metadata) "
          "VALUES ($1, $2, $3, 'negotiation', $4, $5::jsonb)",
          convert::string_to_int(conversation_id).value(),
          convert::string_to_int(current_user_id).value(), negotiation_message,
          negotiation_id, glz::write_json(metadata).value_or(""));

      std::string offer_topic = create_topic("offer", id);
      NotificationMessage msg{
          .type = "offer_accepted",
          .id = id,
          .message = "New Negotiation: " + std::to_string(proposed_price),
          .modified_at = neg_result[0]["created_at"].as<std::string>()};

      ServiceManager::get_instance().get_publisher().publish(
          offer_topic, glz::write_json(msg).value_or(""));

      NegotiateOfferResponse response{.status = "success",
                                      .message = "Negotiation started",
                                      .negotiation_id = negotiation_id,
                                      .conversation_id = conversation_id};

      auto resp =
          HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(response).value_or(""));
      callback(resp);
      LOG_INFO << "Negotiation Transaction committed successfully";
    } catch (const DrogonDbException& e) {
      LOG_ERROR << "Database error in transaction: " << e.base().what();
      transaction->rollback();
      SimpleError error{.error =
                            "Database error: " + std::string(e.base().what())};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
    }
  } catch (const std::exception& e) {
    LOG_ERROR << "Exception: " << e.what();
    SimpleError error{.error = "Error: " + std::string(e.what())};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 获取报价的协商记录
 * @details 获取指定报价的所有价格协商记录，仅报价创建者和帖子所有者有权访问。
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @param id 报价ID字符串
 * @return Task协程对象
 */
// Get negotiations for an offer
Task<> Offers::get_negotiations(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
    std::string id) {
  std::string current_user_id =
      req->getAttributes()->get<std::string>("current_user_id");
  auto db = app().getDbClient();

  try {
    auto result = co_await db->execSqlCoro(
        "SELECT o.*, p.user_id AS post_user_id "
        "FROM offers o "
        "JOIN posts p ON o.post_id = p.id "
        "WHERE o.id = $1",
        convert::string_to_int(id).value());

    if (result.empty()) {
      SimpleError error{.error = "Offer not found"};
      auto resp =
          HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    int offer_user_id = result[0]["user_id"].as<int>();
    int post_user_id = result[0]["post_user_id"].as<int>();

    if (convert::string_to_int(current_user_id).value() != offer_user_id &&
        convert::string_to_int(current_user_id).value() != post_user_id) {
      SimpleError error{.error = "Unauthorized"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    // Get all negotiations for this offer
    auto neg_result = co_await db->execSqlCoro(
        "SELECT pn.*, u.username "
        "FROM price_negotiations pn "
        "JOIN users u ON pn.user_id = u.id "
        "WHERE pn.offer_id = $1 "
        "ORDER BY pn.created_at DESC",
        convert::string_to_int(id).value());

    std::vector<NegotiationInfo> negotiations_data;
    negotiations_data.reserve(neg_result.size());

    for (const auto& row : neg_result) {
      negotiations_data.push_back(NegotiationInfo{
          .id = row["id"].as<int>(),
          .offer_id = row["offer_id"].as<int>(),
          .user_id = row["user_id"].as<int>(),
          .username = row["username"].as<std::string>(),
          .proposed_price = row["proposed_price"].as<double>(),
          .status = row["status"].as<std::string>(),
          .message = row["message"].isNull()
                         ? std::nullopt
                         : std::make_optional(row["message"].as<std::string>()),
          .created_at = row["created_at"].as<std::string>(),
          .updated_at = row["updated_at"].as<std::string>()});
    }

    GetNegotiationsResponse response{.status = "success",
                                     .negotiations = negotiations_data};

    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(response).value_or(""));
    callback(resp);
  } catch (const DrogonDbException& e) {
    SimpleError error{.error =
                          "Database error: " + std::string(e.base().what())};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}

// Partial implementations:

/**
 * @struct RequestProofRequest
 * @brief 请求产品证明请求数据结构
 * @details 包含可选的请求消息。
 */
struct RequestProofRequest {
  std::optional<std::string> message;  // 请求消息（可选）
};

/**
 * @struct RequestProofResponse
 * @brief 请求产品证明响应数据结构
 * @details 返回操作状态、消息和对话ID。
 */
struct RequestProofResponse {
  std::string status;               // 操作状态
  std::string message;              // 响应消息
  std::string conversation_id;      // 对话ID
};

/**
 * @struct SubmitProofRequest
 * @brief 提交产品证明请求数据结构
 * @details 包含证明图片URL和可选描述。
 */
struct SubmitProofRequest {
  std::string image_url;            // 证明图片URL
  std::optional<std::string> description;  // 描述（可选）
};

/**
 * @struct SubmitProofResponse
 * @brief 提交产品证明响应数据结构
 * @details 返回操作状态、消息、证明ID和对话ID。
 */
struct SubmitProofResponse {
  std::string status;               // 操作状态
  std::string message;              // 响应消息
  int proof_id;                     // 证明记录ID
  std::string conversation_id;      // 对话ID
};

/**
 * @struct ProofInfo
 * @brief 产品证明信息数据结构
 * @details 包含产品证明记录的完整信息。
 */
struct ProofInfo {
  int id;                           // 证明记录ID
  int offer_id;                     // 报价ID
  int user_id;                      // 用户ID
  std::string username;             // 用户名
  std::string image_url;            // 证明图片URL
  std::optional<std::string> description;  // 描述（可选）
  std::string status;               // 证明状态
  std::string created_at;           // 创建时间
};

/**
 * @struct GetProofsResponse
 * @brief 获取产品证明列表响应数据结构
 * @details 返回操作状态和证明列表。
 */
struct GetProofsResponse {
  std::string status;               // 操作状态
  std::vector<ProofInfo> proofs;    // 证明记录列表
};

/**
 * @struct RejectProofRequest
 * @brief 拒绝产品证明请求数据结构
 * @details 包含拒绝原因（可选）。
 */
struct RejectProofRequest {
  std::optional<std::string> reason;  // 拒绝原因（可选）
};

/**
 * @struct CreateEscrowRequest
 * @brief 创建托管交易请求数据结构
 * @details 包含托管金额。
 */
struct CreateEscrowRequest {
  double amount;                    // 托管金额
};

/**
 * @struct CreateEscrowResponse
 * @brief 创建托管交易响应数据结构
 * @details 返回操作状态、消息和托管交易ID。
 */
struct CreateEscrowResponse {
  std::string status;               // 操作状态
  std::string message;              // 响应消息
  int escrow_id;                    // 托管交易ID
};

/**
 * @struct EscrowInfo
 * @brief 托管交易信息数据结构
 * @details 包含托管交易的完整信息，包括买卖双方信息。
 */
struct EscrowInfo {
  int id;                           // 托管交易ID
  int offer_id;                     // 报价ID
  int buyer_id;                     // 买家ID
  std::string buyer_username;       // 买家用户名
  int seller_id;                    // 卖家ID
  std::string seller_username;      // 卖家用户名
  double amount;                    // 托管金额
  std::string status;               // 交易状态
  std::string created_at;           // 创建时间
  std::string updated_at;           // 更新时间
};

/**
 * @struct GetEscrowResponse
 * @brief 获取托管交易响应数据结构
 * @details 返回操作状态、是否存在托管交易和托管交易信息。
 */
struct GetEscrowResponse {
  std::string status;               // 操作状态
  bool has_escrow;                  // 是否存在托管交易
  std::optional<EscrowInfo> escrow; // 托管交易信息（可选）
};

/**
 * @brief 请求产品证明
 * @details 帖子所有者请求报价者提供产品证明，在对话中添加证明请求消息。
 * @param req HTTP请求指针，包含可选的message参数
 * @param callback 响应回调函数
 * @param id 报价ID字符串
 * @return Task协程对象
 */
// Request proof of product
Task<> Offers::request_proof(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
    std::string id) {
  std::string current_user_id =
      req->getAttributes()->get<std::string>("current_user_id");
  auto db = app().getDbClient();

  RequestProofRequest proof_req;
  auto parse_error = utilities::strict_read_json(proof_req, req->getBody());

  if (parse_error) {
    SimpleError error{.error = "Invalid JSON"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
    co_return;
  }

  std::string message = proof_req.message.value_or("");

  try {
    auto result = co_await db->execSqlCoro(
        "SELECT o.*, p.user_id AS post_user_id "
        "FROM offers o "
        "JOIN posts p ON o.post_id = p.id "
        "WHERE o.id = $1",
        convert::string_to_int(id).value());

    if (result.empty()) {
      SimpleError error{.error = "Offer not found"};
      auto resp =
          HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    int offer_user_id = result[0]["user_id"].as<int>();
    int post_user_id = result[0]["post_user_id"].as<int>();

    if (convert::string_to_int(current_user_id).value() != post_user_id) {
      SimpleError error{.error = "Only the post owner can request proof"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    std::string conversation_id = co_await create_or_get_conversation(
        current_user_id, std::to_string(offer_user_id), id);

    if (conversation_id.empty()) {
      SimpleError error{.error = "Failed to create conversation"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    std::string proof_message = "Proof of product requested";
    if (!message.empty()) {
      proof_message += "\nMessage: " + message;
    }

    try {
      co_await db->execSqlCoro(
          "INSERT INTO messages "
          "(conversation_id, sender_id, content, context_type, context_id) "
          "VALUES ($1, $2, $3, 'proof_request', $4)",
          convert::string_to_int(conversation_id).value(),
          convert::string_to_int(current_user_id).value(), proof_message,
          convert::string_to_int(id).value());

      RequestProofResponse response{.status = "success",
                                    .message = "Proof requested",
                                    .conversation_id = conversation_id};

      auto resp =
          HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(response).value_or(""));
      callback(resp);
    } catch (const DrogonDbException& e) {
      SimpleError error{.error =
                            "Database error: " + std::string(e.base().what())};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
    }
  } catch (const DrogonDbException& e) {
    SimpleError error{.error =
                          "Database error: " + std::string(e.base().what())};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 提交产品证明
 * @details 报价者提交产品证明图片和描述，创建证明记录，并在对话中添加证明提交消息。
 * @param req HTTP请求指针，包含image_url和可选的description参数
 * @param callback 响应回调函数
 * @param id 报价ID字符串
 * @return Task协程对象
 */
// Submit proof of product
Task<> Offers::submit_proof(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
    std::string id) {
  std::string current_user_id =
      req->getAttributes()->get<std::string>("current_user_id");
  auto db = app().getDbClient();

  SubmitProofRequest submit_proof_req;
  auto parse_error =
      utilities::strict_read_json(submit_proof_req, req->getBody());

  if (parse_error) {
    SimpleError error{.error = "Invalid JSON"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
    co_return;
  }

  std::string image_url = submit_proof_req.image_url;
  std::string description = submit_proof_req.description.value_or("");

  try {
    auto result = co_await db->execSqlCoro(
        "SELECT o.*, p.user_id AS post_user_id "
        "FROM offers o "
        "JOIN posts p ON o.post_id = p.id "
        "WHERE o.id = $1",
        convert::string_to_int(id).value());

    if (result.empty()) {
      SimpleError error{.error = "Offer not found"};
      auto resp =
          HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    int offer_user_id = result[0]["user_id"].as<int>();
    int post_user_id = result[0]["post_user_id"].as<int>();

    if (convert::string_to_int(current_user_id).value() != offer_user_id) {
      SimpleError error{.error = "Only the offer creator can submit proof"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    auto proof_result = co_await db->execSqlCoro(
        "INSERT INTO product_proofs "
        "(offer_id, user_id, image_url, description) "
        "VALUES ($1, $2, $3, $4) "
        "RETURNING id",
        convert::string_to_int(id).value(),
        convert::string_to_int(current_user_id).value(), image_url,
        description);

    if (proof_result.empty()) {
      SimpleError error{.error = "Failed to create proof"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    int proof_id = proof_result[0]["id"].as<int>();

    std::string conversation_id = co_await create_or_get_conversation(
        current_user_id, std::to_string(post_user_id), id);

    if (conversation_id.empty()) {
      SimpleError error{.error = "Failed to create conversation"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    std::string proof_message = "Proof of product submitted";
    if (!description.empty()) {
      proof_message += "\nDescription: " + description;
    }
    proof_message += "\nImage: " + image_url;

    try {
      co_await db->execSqlCoro(
          "INSERT INTO messages "
          "(conversation_id, sender_id, content, context_type, context_id) "
          "VALUES ($1, $2, $3, 'proof_submission', $4)",
          convert::string_to_int(conversation_id).value(),
          convert::string_to_int(current_user_id).value(), proof_message,
          proof_id);

      SubmitProofResponse response{.status = "success",
                                   .message = "Proof submitted",
                                   .proof_id = proof_id,
                                   .conversation_id = conversation_id};

      auto resp =
          HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(response).value_or(""));
      callback(resp);
    } catch (const DrogonDbException& e) {
      SimpleError error{.error =
                            "Database error: " + std::string(e.base().what())};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
    }
  } catch (const DrogonDbException& e) {
    SimpleError error{.error =
                          "Database error: " + std::string(e.base().what())};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 获取报价的产品证明列表
 * @details 获取指定报价的所有产品证明记录，仅报价创建者和帖子所有者有权访问。
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @param id 报价ID字符串
 * @return Task协程对象
 */
// Get proofs for an offer
Task<> Offers::get_proofs(HttpRequestPtr req,
                          std::function<void(const HttpResponsePtr&)> callback,
                          std::string id) {
  std::string current_user_id =
      req->getAttributes()->get<std::string>("current_user_id");
  auto db = app().getDbClient();

  try {
    auto result = co_await db->execSqlCoro(
        "SELECT o.*, p.user_id AS post_user_id "
        "FROM offers o "
        "JOIN posts p ON o.post_id = p.id "
        "WHERE o.id = $1",
        convert::string_to_int(id).value());

    if (result.empty()) {
      SimpleError error{.error = "Offer not found"};
      auto resp =
          HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    int offer_user_id = result[0]["user_id"].as<int>();
    int post_user_id = result[0]["post_user_id"].as<int>();

    if (convert::string_to_int(current_user_id).value() != offer_user_id &&
        convert::string_to_int(current_user_id).value() != post_user_id) {
      SimpleError error{.error = "Unauthorized"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    auto proof_result = co_await db->execSqlCoro(
        "SELECT pp.*, u.username "
        "FROM product_proofs pp "
        "JOIN users u ON pp.user_id = u.id "
        "WHERE pp.offer_id = $1 "
        "ORDER BY pp.created_at DESC",
        convert::string_to_int(id).value());

    std::vector<ProofInfo> proofs_data;
    proofs_data.reserve(proof_result.size());

    for (const auto& row : proof_result) {
      proofs_data.push_back(ProofInfo{
          .id = row["id"].as<int>(),
          .offer_id = row["offer_id"].as<int>(),
          .user_id = row["user_id"].as<int>(),
          .username = row["username"].as<std::string>(),
          .image_url = row["image_url"].as<std::string>(),
          .description =
              row["description"].isNull()
                  ? std::nullopt
                  : std::make_optional(row["description"].as<std::string>()),
          .status = row["status"].as<std::string>(),
          .created_at = row["created_at"].as<std::string>()});
    }

    GetProofsResponse response{.status = "success", .proofs = proofs_data};

    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(response).value_or(""));
    callback(resp);
  } catch (const DrogonDbException& e) {
    SimpleError error{.error =
                          "Database error: " + std::string(e.base().what())};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 批准产品证明
 * @details 帖子所有者批准报价者提交的产品证明，更新证明状态为approved，并在对话中添加批准消息。
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @param id 报价ID字符串
 * @param proof_id 证明记录ID字符串
 * @return Task协程对象
 */
// Approve a proof
Task<> Offers::approve_proof(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
    std::string id, std::string proof_id) {
  std::string current_user_id =
      req->getAttributes()->get<std::string>("current_user_id");
  auto db = app().getDbClient();

  try {
    auto result = co_await db->execSqlCoro(
        "SELECT o.*, p.user_id AS post_user_id "
        "FROM offers o "
        "JOIN posts p ON o.post_id = p.id "
        "WHERE o.id = $1",
        convert::string_to_int(id).value());

    if (result.empty()) {
      SimpleError error{.error = "Offer not found"};
      auto resp =
          HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    int offer_user_id = result[0]["user_id"].as<int>();
    int post_user_id = result[0]["post_user_id"].as<int>();

    if (convert::string_to_int(current_user_id).value() != post_user_id) {
      SimpleError error{.error = "Only the post owner can approve proof"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    auto update_result = co_await db->execSqlCoro(
        "UPDATE product_proofs SET status = 'approved' WHERE id = $1 AND "
        "offer_id = $2 RETURNING id",
        convert::string_to_int(proof_id).value(),
        convert::string_to_int(id).value());

    if (update_result.empty()) {
      SimpleError error{.error = "Proof not found"};
      auto resp =
          HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    std::string conversation_id = co_await create_or_get_conversation(
        current_user_id, std::to_string(offer_user_id), id);

    if (conversation_id.empty()) {
      SimpleError error{.error = "Failed to create conversation"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    std::string approval_message = "Proof of product has been approved";

    try {
      co_await db->execSqlCoro(
          "INSERT INTO messages "
          "(conversation_id, sender_id, content, context_type, context_id) "
          "VALUES ($1, $2, $3, 'proof_approval', $4)",
          convert::string_to_int(conversation_id).value(),
          convert::string_to_int(current_user_id).value(), approval_message,
          convert::string_to_int(proof_id).value());

      StatusResponse response{.status = "success", .message = "Proof approved"};

      auto resp =
          HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(response).value_or(""));
      callback(resp);
    } catch (const DrogonDbException& e) {
      SimpleError error{.error =
                            "Database error: " + std::string(e.base().what())};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
    }
  } catch (const DrogonDbException& e) {
    SimpleError error{.error =
                          "Database error: " + std::string(e.base().what())};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 拒绝产品证明
 * @details 帖子所有者拒绝报价者提交的产品证明，更新证明状态为rejected，并在对话中添加拒绝消息。
 * @param req HTTP请求指针，包含可选的reason参数
 * @param callback 响应回调函数
 * @param id 报价ID字符串
 * @param proof_id 证明记录ID字符串
 * @return Task协程对象
 */
// Reject a proof
Task<> Offers::reject_proof(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
    std::string id, std::string proof_id) {
  std::string current_user_id =
      req->getAttributes()->get<std::string>("current_user_id");
  auto db = app().getDbClient();

  RejectProofRequest reject_req;
  auto parse_error = utilities::strict_read_json(reject_req, req->getBody());

  std::string reason = parse_error ? "" : reject_req.reason.value_or("");

  try {
    auto result = co_await db->execSqlCoro(
        "SELECT o.*, p.user_id AS post_user_id "
        "FROM offers o "
        "JOIN posts p ON o.post_id = p.id "
        "WHERE o.id = $1",
        convert::string_to_int(id).value());

    if (result.empty()) {
      SimpleError error{.error = "Offer not found"};
      auto resp =
          HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    int offer_user_id = result[0]["user_id"].as<int>();
    int post_user_id = result[0]["post_user_id"].as<int>();

    if (convert::string_to_int(current_user_id).value() != post_user_id) {
      SimpleError error{.error = "Only the post owner can reject proof"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    auto update_result = co_await db->execSqlCoro(
        "UPDATE product_proofs SET status = 'rejected' WHERE id = $1 AND "
        "offer_id = $2 RETURNING id",
        convert::string_to_int(proof_id).value(),
        convert::string_to_int(id).value());

    if (update_result.empty()) {
      SimpleError error{.error = "Proof not found"};
      auto resp =
          HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    std::string conversation_id = co_await create_or_get_conversation(
        current_user_id, std::to_string(offer_user_id), id);

    if (conversation_id.empty()) {
      SimpleError error{.error = "Failed to create conversation"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    std::string rejection_message = "Proof of product has been rejected";
    if (!reason.empty()) {
      rejection_message += "\nReason: " + reason;
    }

    try {
      co_await db->execSqlCoro(
          "INSERT INTO messages "
          "(conversation_id, sender_id, content, context_type, context_id) "
          "VALUES ($1, $2, $3, 'proof_rejection', $4)",
          convert::string_to_int(conversation_id).value(),
          convert::string_to_int(current_user_id).value(), rejection_message,
          convert::string_to_int(proof_id).value());

      StatusResponse response{.status = "success", .message = "Proof rejected"};

      auto resp =
          HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(response).value_or(""));
      callback(resp);
    } catch (const DrogonDbException& e) {
      SimpleError error{.error =
                            "Database error: " + std::string(e.base().what())};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
    }
  } catch (const DrogonDbException& e) {
    SimpleError error{.error =
                          "Database error: " + std::string(e.base().what())};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 创建托管交易
 * @details 帖子所有者（买家）创建托管交易，将金额存入托管账户，确保交易安全。
 * @param req HTTP请求指针，包含amount参数
 * @param callback 响应回调函数
 * @param id 报价ID字符串
 * @return Task协程对象
 */
// Create escrow transaction
Task<> Offers::create_escrow(
    HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback,
    std::string id) {
  std::string current_user_id =
      req->getAttributes()->get<std::string>("current_user_id");
  auto db = app().getDbClient();

  CreateEscrowRequest escrow_req;
  auto parse_error = utilities::strict_read_json(escrow_req, req->getBody());

  if (parse_error) {
    SimpleError error{.error = "Invalid JSON"};
    auto resp =
        HttpResponse::newHttpResponse(k400BadRequest, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
    co_return;
  }

  double amount = escrow_req.amount;

  try {
    auto result = co_await db->execSqlCoro(
        "SELECT o.*, p.user_id AS post_user_id "
        "FROM offers o "
        "JOIN posts p ON o.post_id = p.id "
        "WHERE o.id = $1",
        convert::string_to_int(id).value());

    if (result.empty()) {
      SimpleError error{.error = "Offer not found"};
      auto resp =
          HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    int offer_user_id = result[0]["user_id"].as<int>();
    int post_user_id = result[0]["post_user_id"].as<int>();

    if (convert::string_to_int(current_user_id).value() != post_user_id) {
      SimpleError error{.error = "Only the post owner can create escrow"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    auto escrow_result = co_await db->execSqlCoro(
        "INSERT INTO escrow_transactions "
        "(offer_id, buyer_id, seller_id, amount) "
        "VALUES ($1, $2, $3, $4) "
        "RETURNING id",
        convert::string_to_int(id).value(),
        convert::string_to_int(current_user_id).value(), offer_user_id, amount);

    if (escrow_result.empty()) {
      SimpleError error{.error = "Failed to create escrow"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    int escrow_id = escrow_result[0]["id"].as<int>();

    std::string conversation_id = co_await create_or_get_conversation(
        current_user_id, std::to_string(offer_user_id), id);

    if (conversation_id.empty()) {
      SimpleError error{.error = "Failed to create conversation"};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    std::string escrow_message =
        "Escrow transaction created for $" + std::to_string(amount);

    try {
      co_await db->execSqlCoro(
          "INSERT INTO messages "
          "(conversation_id, sender_id, content, context_type, "
          "context_id) "
          "VALUES ($1, $2, $3, 'escrow', $4)",
          convert::string_to_int(conversation_id).value(),
          convert::string_to_int(current_user_id).value(), escrow_message,
          escrow_id);

      CreateEscrowResponse response{.status = "success",
                                    .message = "Escrow created",
                                    .escrow_id = escrow_id};

      auto resp =
          HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(response).value_or(""));
      callback(resp);
    } catch (const DrogonDbException& e) {
      SimpleError error{.error =
                            "Database error: " + std::string(e.base().what())};
      auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                                CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
    }
  } catch (const DrogonDbException& e) {
    SimpleError error{.error =
                          "Database error: " + std::string(e.base().what())};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}

/**
 * @brief 获取报价的托管交易信息
 * @details 获取指定报价的托管交易详情，包括买卖双方信息和交易金额。仅报价创建者和帖子所有者有权访问。
 * @param req HTTP请求指针
 * @param callback 响应回调函数
 * @param id 报价ID字符串
 * @return Task协程对象
 */
// Get escrow transaction for an offer
Task<> Offers::get_escrow(HttpRequestPtr req,
                          std::function<void(const HttpResponsePtr&)> callback,
                          std::string id) {
  std::string current_user_id =
      req->getAttributes()->get<std::string>("current_user_id");
  auto db = app().getDbClient();

  try {
    auto result = co_await db->execSqlCoro(
        "SELECT o.*, p.user_id AS post_user_id "
        "FROM offers o "
        "JOIN posts p ON o.post_id = p.id "
        "WHERE o.id = $1",
        convert::string_to_int(id).value());

    if (result.empty()) {
      SimpleError error{.error = "Offer not found"};
      auto resp =
          HttpResponse::newHttpResponse(k404NotFound, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    int offer_user_id = result[0]["user_id"].as<int>();
    int post_user_id = result[0]["post_user_id"].as<int>();

    if (convert::string_to_int(current_user_id).value() != offer_user_id &&
        convert::string_to_int(current_user_id).value() != post_user_id) {
      SimpleError error{.error = "Unauthorized"};
      auto resp =
          HttpResponse::newHttpResponse(k403Forbidden, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(error).value_or(""));
      callback(resp);
      co_return;
    }

    // Get escrow transaction for this offer
    auto escrow_result = co_await db->execSqlCoro(
        "SELECT et.*, "
        "buyer.username AS buyer_username, "
        "seller.username AS seller_username "
        "FROM escrow_transactions et "
        "JOIN users buyer ON et.buyer_id = buyer.id "
        "JOIN users seller ON et.seller_id = seller.id "
        "WHERE et.offer_id = $1 "
        "ORDER BY et.created_at DESC "
        "LIMIT 1",
        convert::string_to_int(id).value());

    if (escrow_result.empty()) {
      GetEscrowResponse response{.status = "success", .has_escrow = false};

      auto resp =
          HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
      resp->setBody(glz::write_json(response).value_or(""));
      callback(resp);
      co_return;
    }

    const auto& row = escrow_result[0];

    EscrowInfo escrow_info{
        .id = row["id"].as<int>(),
        .offer_id = row["offer_id"].as<int>(),
        .buyer_id = row["buyer_id"].as<int>(),
        .buyer_username = row["buyer_username"].as<std::string>(),
        .seller_id = row["seller_id"].as<int>(),
        .seller_username = row["seller_username"].as<std::string>(),
        .amount = row["amount"].as<double>(),
        .status = row["status"].as<std::string>(),
        .created_at = row["created_at"].as<std::string>(),
        .updated_at = row["updated_at"].as<std::string>()};

    GetEscrowResponse response{
        .status = "success", .has_escrow = true, .escrow = escrow_info};

    auto resp =
        HttpResponse::newHttpResponse(drogon::k200OK, CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(response).value_or(""));
    callback(resp);
  } catch (const DrogonDbException& e) {
    SimpleError error{.error =
                          "Database error: " + std::string(e.base().what())};
    auto resp = HttpResponse::newHttpResponse(k500InternalServerError,
                                              CT_APPLICATION_JSON);
    resp->setBody(glz::write_json(error).value_or(""));
    callback(resp);
  }

  co_return;
}
