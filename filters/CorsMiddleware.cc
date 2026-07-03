/**
 * @file CorsMiddleware.cc
 * @brief CORS跨域中间件实现文件
 * @details 实现跨域资源共享(CORS)功能，限制仅允许来自本地主机的请求，
 *          在响应中添加必要的CORS头部信息。
 */

#include <drogon/HttpMiddleware.h>    // 引入Drogon HTTP中间件头文件
using namespace drogon;    // 使用Drogon命名空间

/**
 * @brief CORS middleware for handling cross-origin requests 用于处理跨源请求的
 * CORS 中间件
 */
class CorsMiddleware : public HttpMiddleware<CorsMiddleware> {
 public:
  CorsMiddleware() {};    // 默认构造函数

  /**
   * @brief 中间件调用方法
   * @param req HTTP请求指针
   * @param nextCb 下一个中间件的回调函数
   * @param mcb 中间件完成回调函数
   * @details 检查请求来源，仅允许本地主机请求，在响应中添加CORS头部信息。
   */
  void invoke(const HttpRequestPtr &req, MiddlewareNextCallback &&nextCb,
              MiddlewareCallback &&mcb) override {
    const std::string &origin = req->getHeader("origin");    // 获取请求来源头
    /*这意味着后端服务只能从本机访问：
      1.允许的请求：
        在同一台服务器上运行的前端应用
        本地测试工具（Postman、curl 等）
        通过 localhost 或 127.0.0.1 发起的请求
      2.被拒绝的请求：
        来自其他电脑的访问
        来自局域网内其他设备的请求
        来自互联网的请求
        即使是同一局域网内的 192.168.1.100 也会被拦截
     */
    // Only allow requests from localhost 允许来自 localhost 的请求
    if (!req->peerAddr().isLoopbackIp()) {
      // intercept directly 直接拦截
      LOG_TRACE << "Intercepted request from non-localhost origin: " << origin;    // 记录拦截日志
      mcb(HttpResponse::newNotFoundResponse(req));// 返回 404 错误
      return;
    }
    LOG_TRACE << "Allowing request from origin: " << origin;    // 记录允许请求日志
    // Do something before calling the next middleware 在调用下一个中间件之前先做些事情
    nextCb([&origin, mcb = std::move(mcb)](const HttpResponsePtr &resp) {
      // Do something after the next middleware returns 在下一个中间件返回之后再执行某项操作
      resp->addHeader("Access-Control-Allow-Origin", origin);// 设置访问控制允许来源
      resp->addHeader("Access-Control-Allow-Credentials", "true");// 设置访问控制允许凭证
      resp->addHeader("Access-Control-Allow-Methods",
                      "GET, POST, PUT, DELETE, OPTIONS");// 设置访问控制允许方法
      resp->addHeader("Access-Control-Allow-Headers",
                      "Content-Type, Authorization");// 设置访问控制允许的请求头
      mcb(resp);// 调用下一个中间件
    });
  }
};
