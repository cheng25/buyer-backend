/**
 * @file conversion.hpp
 * @brief 类型转换工具头文件
 * @details 提供字符串与数字、数组格式转换等工具函数。
 */

#ifndef CONVERSION_HPP
#define CONVERSION_HPP

#include <charconv>                  // 引入字符转换头文件
#include <concepts>                  // 引入概念头文件
#include <optional>                  // 引入可选类型头文件
#include <span>                      // 引入跨度头文件
#include <string>                    // 引入字符串类头文件
#include <string_view>               // 引入字符串视图头文件
#include <vector>                    // 引入向量容器头文件

namespace convert {

/**
 * @brief 将字符串转换为整数
 * @param sv 字符串视图
 * @return 转换后的整数（如果成功），否则返回std::nullopt
 * @details 使用std::from_chars进行高效的字符串到整数转换，确保整个字符串都被解析。
 */
/* constexpr */ inline std::optional<int> string_to_int(std::string_view sv) {
  int value;
  const auto ret = std::from_chars(sv.data(), sv.data() + sv.size(), value);    // 使用from_chars转换
  if (ret.ec == std::errc{} && ret.ptr == sv.data() + sv.size()) return value;    // 检查转换是否成功

  return std::nullopt;    // 转换失败返回空
};

/**
 * @brief 数值类型概念
 * @tparam T 类型参数
 * @details 约束类型必须是算术类型（整数或浮点数）。
 */
template <class T>
concept Numeric = std::is_arithmetic_v<T>;

/**
 * @brief 将字符串转换为数值类型
 * @tparam T 数值类型（必须满足Numeric概念）
 * @param sv 字符串视图
 * @return 转换后的数值（如果成功），否则返回std::nullopt
 * @details 使用std::from_chars进行高效的字符串到数值转换，支持多种数值类型。
 */
template <Numeric T>
inline std::optional<T> string_to_number(std::string_view sv) {
  std::optional<T> value{{}};  // default init T 默认初始化T
  auto ret = std::from_chars(sv.data(), sv.data() + sv.size(), *value);    // 使用from_chars转换
  if (ret.ec == std::errc{} && ret.ptr == sv.data() + sv.size()) return value;    // 检查转换是否成功
  return std::nullopt;    // 转换失败返回空
}

// Array of strings to PostgreSQL array string
// 字符串数组转换为PostgreSQL数组字符串
// if empty returns "{}". 如果为空返回"{}"。

/**
 * @brief 将字符串数组转换为PostgreSQL数组字符串
 * @param tags 字符串跨度
 * @return PostgreSQL数组格式的字符串（如"{tag1,tag2,tag3}"）
 * @details 如果输入为空，返回"{}"；否则将字符串连接成PostgreSQL数组格式。
 */
inline std::string array_to_pgsql_array_string(
    std::span<const std::string> tags) {
  if (tags.empty()) {
    return "{}";    // 空数组返回"{}"
  }

  std::string result = "{";    // 初始化结果字符串
  for (size_t i{0}; const auto& tag : tags) {
    if (i > 0) {
      result += ",";    // 添加逗号分隔符
    }
    result += tag;    // 添加当前标签
    ++i;
  }
  result += "}";    // 闭合数组

  return result;
}

// PostgresSQL array string to std::vector<std::string>
// PostgreSQL数组字符串转换为std::vector<std::string>

/**
 * @brief 将PostgreSQL数组字符串转换为字符串向量
 * @param array_str PostgreSQL数组格式的字符串
 * @return 字符串向量
 * @details 解析PostgreSQL数组格式"{tag1,tag2,tag3}"，提取其中的元素并存储到向量中。
 */
inline std::vector<std::string> pgsql_array_string_to_vector(
    const std::string& array_str) {
  std::vector<std::string> result;

  if (array_str.size() < 2) {
    return result;    // 字符串太短，无法解析
  }

  // Parse PostgreSQL array format: {tag1,tag2,tag3}, remove {}
  // 解析PostgreSQL数组格式：{tag1,tag2,tag3}，移除{}
  auto content = std::string_view(array_str.begin() + 1, array_str.end() - 1);    // 提取内容部分

  size_t count = std::count(content.begin(), content.end(), ',') + 1;    // 计算元素数量
  result.reserve(count);    // 预分配空间

  size_t start = 0;
  size_t end = content.find(',');
  while (end != std::string::npos) {
    result.emplace_back(content.substr(start, end - start));    // 添加当前元素
    start = end + 1;    // 移动到下一个元素起始位置
    end = content.find(',', start);    // 查找下一个逗号
  }
  if (!content.substr(start).empty()) {
    result.emplace_back(content.substr(start));  // Add the last substring 添加最后一个元素
  }

  return result;
}

}  // namespace convert

#endif
