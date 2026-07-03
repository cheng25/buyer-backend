/**
 * @file validation.hpp
 * @brief 数据验证工具头文件
 * @details 提供数据验证相关的工具函数，如邮箱地址验证等。
 */

#ifndef VALIDATION_HPP
#define VALIDATION_HPP

#include <regex>                    // 引入正则表达式头文件
#include <string>                   // 引入字符串类头文件

namespace utilities {

/**
 * TODO: Test invalid email format 测试无效邮箱格式
 * To validate email addresses, we would need to conform to RFC 5322 format.
 * 要验证邮箱地址，我们需要符合RFC 5322格式。
 * Some alternatives include, relying on frontend that uses a fully defined
 * parser e.g. https://github.com/jackbearheart/email-addresses
 * 一些替代方案包括依赖使用完全定义解析器的前端，例如
 * https://github.com/jackbearheart/email-addresses
 * or creating an RFC 5322 compliant Boost.parser/Lexy email address parser
 * and using it.
 * 或者创建一个符合RFC 5322的Boost.parser/Lexy邮箱地址解析器并使用它。
 * As we rely on verifying email with tokens, this may not be
 * bad for normal application functionality.
 * 由于我们依赖令牌验证邮箱，这对于正常的应用功能可能不是坏事。
 * But a good validator prevent DDos with invalid emails and other issues.
 * 但是一个好的验证器可以防止使用无效邮箱的DDoS攻击和其他问题。
 * And no don't rely on Regex:
 * 不要依赖正则表达式：
 * https://stackoverflow.com/questions/201323/how-can-i-validate-an-email-address-using-a-regular-expression
 */

// Quick and dirty RFC 5322 compliant email validator (basic)
// 简单的RFC 5322兼容邮箱验证器（基础版）

/**
 * @brief 验证邮箱地址是否有效
 * @param email 邮箱地址字符串
 * @return 如果邮箱格式有效返回true，否则返回false
 * @details 使用正则表达式验证邮箱地址格式，基本覆盖大多数常见邮箱格式，但不完全符合RFC 5322标准。
 */
inline bool is_email_valid(const std::string& email) {
  // Very basic regex for demonstration, not fully RFC compliant but covers most
  // cases
  // 非常基础的正则表达式用于演示，不完全符合RFC标准但覆盖大多数情况
  // see https://stackoverflow.com/a/14075810 for a more detailed one
  // 查看https://stackoverflow.com/a/14075810获取更详细的正则表达式
  // but it is nots yet implementable in C++ ?
  // 但它在C++中尚未实现？
  // Basic email regex: local@domain.tld (not fully RFC 5322 compliant)
  // 基本邮箱正则：local@domain.tld（不完全符合RFC 5322标准）
  const std::regex pattern(
      R"(^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$)");    // 邮箱验证正则表达式

  return std::regex_match(email, pattern);    // 使用正则表达式匹配邮箱
}

}  // namespace utilities

#endif  // VALIDATION_HPP
