#ifndef JSON_MANIPULATION_HPP
#define JSON_MANIPULATION_HPP

#include <glaze/glaze.hpp>
#include <optional>
#include <string>

namespace utilities {

/**
 * @brief Strict read JSON that requires all keys to be present and does not
 * allow unknown keys.严格读取JSON，要求所有键都存在，不允许未知键。
 * @tparam T The type to read into. 读取的类型。
 * @param value The value to read into. Of type T.读取的值。
 * @param buffer The input buffer containing the JSON data.输入缓冲区，包含JSON数据。
 * @return glz::error_ctx if there is an error, otherwise returns the parsed value.
 * 如果发生错误，则返回glz::error_ctx，否则返回解析后的值。
 */
template <glz::read_supported<glz::JSON> T, glz::is_buffer Buffer>
[[nodiscard]] inline glz::error_ctx strict_read_json(T &value,
                                                     Buffer &&buffer) {
  glz::context ctx{};
  // .error_on_unknown_keys = true,拒绝任何不包含目标结构体中定义的所有字段的 JSON 请求体。防止不完整的
  return read<glz::opts{.error_on_missing_keys = true}>(
      value, std::forward<Buffer>(buffer), ctx);
}

/**
 * @brief Strict read JSON that requires all keys to be present and does not
 * allow unknown keys. 严格读取JSON，要求所有键都存在，不允许未知键。
 * @tparam T The type to read into. 读取的类型。
 * @param buffer The input buffer containing the JSON data.输入缓冲区，包含JSON数据。
 * @return glz::expected containing the parsed value of type T or an error context
 * 如果发生错误，则返回glz::error_ctx，否则返回解析后的值。包含解析值的类型T或错误上下文
 */
template <glz::read_supported<glz::JSON> T, glz::is_buffer Buffer>
[[nodiscard]] inline glz::expected<T, glz::error_ctx> strict_read_json(
    Buffer &&buffer) {
  T value{};
  glz::context ctx{};
  const glz::error_ctx ec = read<glz::opts{.error_on_missing_keys = true}>(
      value, std::forward<Buffer>(buffer), ctx);
  if (ec) {
    return glz::unexpected<glz::error_ctx>(ec);
  }
  return value;
}

/**
 * @brief Relaxed read JSON that allows unknown keys and missing keys.
 * @tparam T The type to read into.
 * @param value The value to read into. Of type T.
 * @param buffer The input buffer containing the JSON data.
 * @return glz::error_ctx if there is an error, otherwise returns the parsed
 * value
 */
template <glz::read_supported<glz::JSON> T, glz::is_buffer Buffer>
[[nodiscard]] inline glz::error_ctx relaxed_read_json(T &value,
                                                      Buffer &&buffer) {
  glz::context ctx{};
  // .error_on_missing_keys = false (default)
  return read<glz::opts{.error_on_unknown_keys = false}>(
      value, std::forward<Buffer>(buffer), ctx);
}

/**
 * @brief Relaxed read JSON that allows unknown keys and missing keys.
 * @tparam T The type to read into.
 * @param buffer The input buffer containing the JSON data.
 * @return glz::expected containing the parsed value of type T or an error
 * context
 */
template <glz::read_supported<glz::JSON> T, glz::is_buffer Buffer>
[[nodiscard]] inline glz::expected<T, glz::error_ctx> relaxed_read_json(
    Buffer &&buffer) {
  T value{};
  glz::context ctx{};
  const glz::error_ctx ec = read<glz::opts{.error_on_unknown_keys = false}>(
      value, std::forward<Buffer>(buffer), ctx);
  if (ec) {
    return glz::unexpected<glz::error_ctx>(ec);
  }
  return value;
}

}  // namespace utilities

#endif  // JSON_MANIPULATION_HPP
