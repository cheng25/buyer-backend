/**
 * @file uuid_generator.hpp
 * @brief UUID生成器头文件
 * @details 提供UUID生成功能，基于Boost库实现。
 */

#ifndef UUID_GENERATOR_HPP
#define UUID_GENERATOR_HPP

#include <boost/uuid/uuid.hpp>              // 引入Boost UUID头文件
#include <boost/uuid/uuid_generators.hpp>   // 引入Boost UUID生成器头文件
#include <boost/uuid/uuid_io.hpp>           // 引入Boost UUID IO头文件

/**
 * @class UuidGenerator
 * @brief UUID生成器类
 * @details 使用Boost库生成随机UUID（版本4）。
 */
class UuidGenerator {
 public:
  /**
   * @brief 生成UUID字符串
   * @return UUID字符串（格式如：xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx）
   * @details 使用随机生成器生成UUID并转换为字符串格式。
   */
  static std::string generate_uuid() {
    boost::uuids::random_generator generator;    // 创建随机UUID生成器
    boost::uuids::uuid uuid = generator();       // 生成UUID
    return boost::uuids::to_string(uuid);        // 将UUID转换为字符串
  }
};

#endif  // UUID_GENERATOR_HPP
