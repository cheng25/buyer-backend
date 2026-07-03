/**
 * @file json_manipulation.hpp
 * @brief JSON操作工具函数头文件
 * @details 提供基于glaze库的JSON解析工具函数，支持严格模式和宽松模式两种解析方式。
 */
#ifndef JSON_MANIPULATION_HPP
#define JSON_MANIPULATION_HPP

#include <glaze/glaze.hpp>   // glaze JSON库
#include <optional>          // 可选类型
#include <string>            // 字符串类型

namespace utilities {

/**
 * @brief 严格模式读取JSON（引用参数版本）
 * @details 严格读取JSON，要求目标结构体中定义的所有键都必须存在，缺失任何键都会导致解析失败。
 * 拒绝任何不包含目标结构体中定义的所有字段的JSON请求体，防止不完整的数据提交。
 * @tparam T 目标类型，必须支持glaze的JSON读取
 * @tparam Buffer 输入缓冲区类型
 * @param value 解析结果将写入此引用参数
 * @param buffer 包含JSON数据的输入缓冲区
 * @return glz::error_ctx 如果发生错误则返回错误上下文，否则返回空表示成功
 */
template <glz::read_supported<glz::JSON> T, glz::is_buffer Buffer>
[[nodiscard]] inline glz::error_ctx strict_read_json(T &value,
                                                     Buffer &&buffer) {
  glz::context ctx{};  // glz解析上下文
  // 设置error_on_missing_keys = true，严格要求所有键必须存在
  return read<glz::opts{.error_on_missing_keys = true}>(
      value, std::forward<Buffer>(buffer), ctx);
}

/**
 * @brief 严格模式读取JSON（返回值版本）
 * @details 严格读取JSON，要求目标结构体中定义的所有键都必须存在。
 * 解析成功返回包含解析值的expected，解析失败返回错误上下文。
 * @tparam T 目标类型，必须支持glaze的JSON读取
 * @tparam Buffer 输入缓冲区类型
 * @param buffer 包含JSON数据的输入缓冲区
 * @return glz::expected<T, glz::error_ctx> 包含解析后的值（类型T）或错误上下文
 */
template <glz::read_supported<glz::JSON> T, glz::is_buffer Buffer>
[[nodiscard]] inline glz::expected<T, glz::error_ctx> strict_read_json(
    Buffer &&buffer) {
  T value{};                       // 默认构造目标对象
  glz::context ctx{};              // glz解析上下文
  // 设置error_on_missing_keys = true，严格要求所有键必须存在
  const glz::error_ctx ec = read<glz::opts{.error_on_missing_keys = true}>(
      value, std::forward<Buffer>(buffer), ctx);
  if (ec) {
    return glz::unexpected<glz::error_ctx>(ec);  // 解析失败，返回错误
  }
  return value;                    // 解析成功，返回解析后的值
}

/**
 * @brief 宽松模式读取JSON（引用参数版本）
 * @details 宽松读取JSON，允许未知键和缺失键。缺失的键使用目标类型的默认值，
 * 未知的键会被忽略，不会导致解析失败。
 * @tparam T 目标类型，必须支持glaze的JSON读取
 * @tparam Buffer 输入缓冲区类型
 * @param value 解析结果将写入此引用参数
 * @param buffer 包含JSON数据的输入缓冲区
 * @return glz::error_ctx 如果发生错误则返回错误上下文，否则返回空表示成功
 */
template <glz::read_supported<glz::JSON> T, glz::is_buffer Buffer>
[[nodiscard]] inline glz::error_ctx relaxed_read_json(T &value,
                                                      Buffer &&buffer) {
  glz::context ctx{};  // glz解析上下文
  // error_on_missing_keys = false（默认），允许缺失键；error_on_unknown_keys = false，允许未知键
  return read<glz::opts{.error_on_unknown_keys = false}>(
      value, std::forward<Buffer>(buffer), ctx);
}

/**
 * @brief 宽松模式读取JSON（返回值版本）
 * @details 宽松读取JSON，允许未知键和缺失键。缺失的键使用目标类型的默认值，
 * 未知的键会被忽略。解析成功返回包含解析值的expected，解析失败返回错误上下文。
 * @tparam T 目标类型，必须支持glaze的JSON读取
 * @tparam Buffer 输入缓冲区类型
 * @param buffer 包含JSON数据的输入缓冲区
 * @return glz::expected<T, glz::error_ctx> 包含解析后的值（类型T）或错误上下文
 */
template <glz::read_supported<glz::JSON> T, glz::is_buffer Buffer>
[[nodiscard]] inline glz::expected<T, glz::error_ctx> relaxed_read_json(
    Buffer &&buffer) {
  T value{};                       // 默认构造目标对象
  glz::context ctx{};              // glz解析上下文
  // error_on_missing_keys = false（默认），允许缺失键；error_on_unknown_keys = false，允许未知键
  const glz::error_ctx ec = read<glz::opts{.error_on_unknown_keys = false}>(
      value, std::forward<Buffer>(buffer), ctx);
  if (ec) {
    return glz::unexpected<glz::error_ctx>(ec);  // 解析失败，返回错误
  }
  return value;                    // 解析成功，返回解析后的值
}

}  // namespace utilities

#endif  // JSON_MANIPULATION_HPP
