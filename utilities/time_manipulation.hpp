/**
 * @file time_manipulation.hpp
 * @brief 时间操作工具头文件
 * @details 提供时间相关的工具函数，如获取SQL兼容的UTC时间戳等。
 */

#ifndef TIME_MANIPULATION_HPP
#define TIME_MANIPULATION_HPP

#include <chrono>                    // 引入时间库头文件
#include <format>                    // 引入格式化输出头文件
#include <string>                    // 引入字符串类头文件

/**
 * Gets SQL compatible UTC timestamp with microseconds precision
 * 获取SQL兼容的带微秒精度的UTC时间戳
 * Format: YYYY-MM-DD HH:MM:SS.ssssss
 * 格式：YYYY-MM-DD HH:MM:SS.ssssss
 */

/**
 * @brief 获取带微秒精度的UTC时间戳
 * @return SQL兼容格式的时间戳字符串（YYYY-MM-DD HH:MM:SS.ssssss）
 * @details 获取当前UTC时间，精确到微秒，并格式化为SQL兼容的字符串格式。
 */
inline std::string get_precise_sql_utc_timestamp() {
  auto now = std::chrono::utc_clock::now();    // 获取当前UTC时间
  auto seconds = std::chrono::floor<std::chrono::seconds>(now);    // 获取秒部分
  auto us =
      std::chrono::duration_cast<std::chrono::microseconds>(now - seconds);    // 获取微秒部分
  return std::format("{}.{:06}", std::format("{:%F %T}", seconds), us.count());    // 格式化为SQL兼容的时间戳
}

#endif  // TIME_MANIPULATION_HPP
