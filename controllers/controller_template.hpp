/**
 * @file controller_template.hpp
 * @brief Drogon HTTP控制器模板头文件
 * @details 提供控制器类的模板结构，包含METHOD_LIST_BEGIN和METHOD_LIST_END宏定义示例，便于创建新的API控制器。
 */
#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace api
{
namespace v1
{
/**
 * @class ControllerTemplate
 * @brief Drogon HTTP控制器模板类
 * @details 继承自drogon::HttpController，提供控制器类的基本结构和路由注册示例。
 * @note 此类作为创建新控制器的参考模板，实际使用时应继承此类并重写方法。
 */
class ControllerTemplate : public drogon::HttpController<ControllerTemplate>
{
  public:
    /**
     * @brief 路由方法列表开始宏
     * @details 使用METHOD_ADD宏注册自定义处理函数到路由表。
     */
    METHOD_LIST_BEGIN
    // use METHOD_ADD to add your custom processing function here;
    // METHOD_ADD(ControllerTemplate::get, "/{2}/{1}", Get); // path is /api/v1/ControllerTemplate/{arg2}/{arg1}
    // METHOD_ADD(ControllerTemplate::your_method_name, "/{1}/{2}/list", Get); // path is /api/v1/ControllerTemplate/{arg1}/{arg2}/list
    // ADD_METHOD_TO(ControllerTemplate::your_method_name, "/absolute/path/{1}/{2}/list", Get); // path is /absolute/path/{arg1}/{arg2}/list

    // Using LocalHost Filer??
    // METHOD_ADD(ControllerTemplate::login, "/login", Post, Options, "drogon::LocalHostFilter");

    /**
     * @brief 路由方法列表结束宏
     * @details 标记路由注册结束。
     */
    METHOD_LIST_END
    // your declaration of processing function maybe like this:
    // void get(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, int p1, std::string p2);
    // void your_method_name(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, double p1, int p2) const;
};
}
}
