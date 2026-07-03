/**
 * @file helpers.hpp
 * @brief 测试辅助工具头文件
 * @details 提供测试过程中使用的辅助函数，如数据库清理等。
 */

#include <drogon/drogon.h>           // 引入Drogon框架核心头文件

namespace helpers {

/**
 * @brief 清理数据库测试数据
 * @details 删除测试过程中产生的所有数据，包括用户订阅、通知、位置、对话、帖子、媒体、订单和用户。
 */
inline void cleanup_db() {
  try {
    auto db_client = drogon::app().getDbClient();    // 获取数据库客户端

    db_client->execSqlSync("DELETE from user_subscriptions");    // 删除用户订阅表
    db_client->execSqlSync("DELETE from notifications");         // 删除通知表
    db_client->execSqlSync("DELETE from locations");             // 删除位置表
    db_client->execSqlSync("DELETE from conversations");         // 删除对话表
    db_client->execSqlSync("DELETE from posts");                 // 删除帖子表
    db_client->execSqlSync("DELETE from media");                 // 删除媒体表
    db_client->execSqlSync("DELETE from orders");                // 删除订单表
    db_client->execSqlSync("DELETE from users");                 // 删除用户表
  } catch (...) {
    // 忽略异常，确保测试不会因清理失败而中断
  }
}

}  // namespace helpers