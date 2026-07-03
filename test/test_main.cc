/**
 * @file test_main.cc
 * @brief 测试程序主入口文件
 * @details 负责初始化测试环境、加载配置文件、启动Drogon应用并运行所有测试用例。
 */

#define DROGON_TEST_MAIN             // 定义Drogon测试主入口宏
#include <drogon/drogon.h>           // 引入Drogon框架核心头文件
#include <drogon/drogon_test.h>      // 引入Drogon测试框架头文件

#include <array>                     // 引入数组容器头文件
#include <filesystem>                // 引入文件系统头文件
#include <iostream>                  // 引入输入输出流头文件

// Helper function to find the test_config.json file
// 查找test_config.json配置文件的辅助函数

/**
 * @brief 查找测试配置文件
 * @return 配置文件路径
 * @details 按优先级从多个可能的路径中查找test_config.json文件。
 */
std::string find_config_file() {
  // Try different relative paths from the executable location
  // 尝试从可执行文件位置的不同相对路径查找
  const std::array<std::string, 6> possible_paths = {
      "test_config.json",           // Same directory 同一目录
      "../test_config.json",        // One level up 上一级目录
      "../../test_config.json",     // Two levels up 上两级目录
      "../../../test_config.json",  // Three levels up 上三级目录
      "test/test_config.json",      // In test subdirectory test子目录
      "../test/test_config.json"    // In test subdirectory one level up 上一级的test子目录
  };

  for (const auto& path : possible_paths) {
    if (std::filesystem::exists(path)) {
      std::cout << "Found config file at: " << path << std::endl;
      return path;
    }
  }

  // If not found, default to the original path and log a warning
  // 如果未找到，使用默认路径并记录警告
  std::cerr << "Warning: Could not find test_config.json in any of the "
               "expected locations."
            << std::endl;
  std::cerr << "Will try with ../../test_config.json" << std::endl;
  return "../../test_config.json";
}

/**
 * @brief 测试程序主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 测试运行状态码
 * @details 加载配置文件、启动Drogon应用、运行测试用例并关闭应用。
 */
int main(int argc, char** argv) {
  //  Note that "../../../test_config.json" is relative to the executable in
  //  MSVC
  // 注意：在MSVC中，"../../../test_config.json"是相对于可执行文件的路径

  // Find and load test configuration 查找并加载测试配置
  std::string config_path = find_config_file();    // 查找配置文件路径
  std::cout << "Loading configuration from: " << config_path << std::endl;

  try {
    drogon::app().loadConfigFile(config_path);    // 加载配置文件
  } catch (const std::exception& e) {
    std::cerr << "Error loading config file: " << e.what() << std::endl;
    std::cerr << "Working directory: " << std::filesystem::current_path()
              << std::endl;
    return 1;    // 配置加载失败，返回错误码
  }

  std::promise<void> p1;
  std::future<void> f1 = p1.get_future();

  // Start the main loop on another thread 在另一个线程启动主事件循环
  std::thread thr([&]() {
    // Queues the promise to be fulfilled after starting the loop
    // 将promise排队到事件循环中，在循环启动后完成
    drogon::app().getLoop()->queueInLoop([&p1]() { p1.set_value(); });
    drogon::app().run();    // 运行Drogon应用
  });

  // The future is only satisfied after the event loop started
  // future只在事件循环启动后才会被满足
  f1.get();    // 等待事件循环启动完成
  const int status = drogon::test::run(argc, argv);    // 运行测试用例
  std::this_thread::sleep_for(std::chrono::milliseconds(
      1500));  // prevents seg faults in release builds. // 防止release构建中的段错误

  // Ask the event loop to shutdown and wait 请求事件循环关闭并等待
  drogon::app().getLoop()->queueInLoop([]() { drogon::app().quit(); });
  thr.join();    // 等待线程结束
  return status;
}
