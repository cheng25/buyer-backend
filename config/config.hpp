/**
 * @file config.hpp
 * @brief 配置管理模块头文件
 * @details 提供配置值获取的辅助函数和全局配置常量定义。
 */

#pragma once
#include <drogon/drogon.h>    // 引入Drogon Web框架头文件

#include <string>             // 引入字符串类头文件

/**
 * @namespace config
 * @brief 配置管理命名空间
 * @details 包含配置值获取函数和全局配置常量。
 */
namespace config {

/**
 * @brief 获取配置值函数
 * @param key 配置键名
 * @param default_value 默认值
 * @return 配置值，如果键不存在则返回默认值
 * @details 从Drogon应用的自定义配置中获取指定键的值，
 *          如果键不存在则返回提供的默认值。
 */
inline std::string get_config_value(const std::string &key,
                                    const std::string &default_value) {
  const Json::Value &config = drogon::app().getCustomConfig();    // 获取Drogon应用的自定义配置
  if (config.isMember(key)) {
    return config[key].asString();    // 返回配置值的字符串形式
  }
  return default_value;               // 返回默认值
}

// Only fetch once 只获取一次配置值
/**
 * @brief JWT密钥常量
 * @details 用于JWT令牌签名和验证的密钥，程序启动时从配置文件中读取，
 *          如果配置文件中不存在则使用默认值"default_secret"。
 */
static inline const std::string JWT_SECRET =
    get_config_value("jwt_secret", "default_secret");

}  // namespace config
