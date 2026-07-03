/**
 * @file custom_macro.hpp
 * @brief 自定义宏头文件
 * @details 提供项目中使用的自定义宏定义。
 */

#ifndef CUSTOM_MACRO_HPP
#define CUSTOM_MACRO_HPP

/*
   Avoid "unused parameter" warnings
   避免“未使用的参数”警告
*/

/**
 * @brief 避免未使用参数警告的宏
 * @param x 未使用的参数
 * @details 将参数转换为void表达式，避免编译器产生"unused parameter"警告。
 */
#define Q_UNUSED(x) (void)x;

#endif  // CUSTOM_MACRO_HPP
