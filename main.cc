/**
 * @file main.cc
 * @brief 买家后端服务主入口文件
 * @details 该文件包含应用程序的主函数，负责初始化和启动Drogon Web服务器，
 *          加载配置文件，初始化服务管理器，并设置HTTP监听器。
 */

#include <drogon/drogon.h>    // 引入Drogon Web框架头文件

#include <array>              // 引入数组容器头文件
#include <filesystem>         // 引入文件系统操作头文件
#include <format>             // 引入格式化输出头文件
#include <iostream>           // 引入输入输出流头文件
#include <string>             // 引入字符串类头文件

#include "services/service_manager.hpp"    // 引入服务管理器头文件

/**
 * @brief 打印帮助信息函数
 * @details 向标准输出打印命令行参数的使用说明，包括可用选项及其功能描述。
 */
void print_help() {
  std::cout << "Usage: buyer-backend [OPTIONS]\n\n"    // 输出使用格式
               "Options:\n"                          // 输出选项标题
               "  --test, -t       Run in test mode using test_config.json.\n"
               "                   Searches up to 3 parent directories up.\n"
               "  --config <file>  Use specified config file.\n"
               "                   Searches up to 3 parent directories up.\n"
               "  --help, -h       Display this help message and exit.\n";
}

/**
 * @brief 查找配置文件函数
 * @param filename 配置文件名
 * @return 配置文件的完整路径，如果未找到则返回原始文件名
 * @details 在当前目录及其最多3层父目录中搜索指定的配置文件，
 *          如果找到则返回完整路径，否则返回原始文件名并输出警告。
 */
std::string find_config_file(const std::string& filename) {
  // Search up to 3 parent directories up. 搜索最多3个父目录
  // 构造可能的配置文件路径数组，包括当前目录和向上3层父目录
  const std::array<std::string, 4> possible_paths = {
      filename, "../" + filename, "../../" + filename, "../../../" + filename};

  // 遍历所有可能的路径，查找配置文件是否存在
  for (const auto& path : possible_paths) {
    if (std::filesystem::exists(path)) {
      std::puts(std::format("Found config file at: {}", path).c_str());
      return path;
    }
  }

  // If not found, return the original path and log a warning 如果未找到，返回原始路径并记录警告
  std::cerr << std::format(
      "Warning: Could not find {} in any of the expected locations.", filename);
  std::cerr << std::format("Will try with {} directly.", filename);
  return filename;
}

/**
 * @brief 主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码，0表示成功，非0表示失败
 * @details 应用程序入口点，负责解析命令行参数、配置HTTP监听器、
 *          加载配置文件、初始化服务管理器、创建媒体存储桶，
 *          并启动Drogon Web服务器。
 */
int main(int argc, char* argv[]) {
  bool test_mode = false;      // 测试模式标志，默认为false
  std::string config;          // 配置文件路径字符串

  // Parse command line arguments 处理命令行参数
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];    // 将当前参数转换为字符串
    if (arg == "--test" || arg == "-t") {
      test_mode = true;           // 设置为测试模式
      std::puts("Running in test mode");
    } else if (arg == "--config" && i + 1 < argc) {
      config = argv[i + 1];       // 获取用户指定的配置文件路径
      i++;  // Skip the next argument 跳过下一个参数（配置文件名）
      std::puts(std::format("Using config: {}", config).c_str());
    } else if (arg == "--help" || arg == "-h") {
      print_help();               // 打印帮助信息
      return 0;                   // 正常退出
    } else {
      std::puts(std::format("Unknown option: {}", arg).c_str());
      print_help();               // 打印帮助信息
      return 1;                   // 错误退出
    }
  }

  // Set HTTP listener address and port 设置HTTP监听器地址和端口
  drogon::app().addListener("0.0.0.0", 5555);    // 监听所有网络接口的5555端口
  // Load config file and set up services 加载配置文件并设置服务
  try {
    if (test_mode) {
      // 测试模式下查找并加载测试配置文件
      std::string test_config_path = find_config_file("test_config.json");
      std::puts(
          std::format("Loading test configuration from: {}", test_config_path)
              .c_str());
      drogon::app().loadConfigFile(test_config_path);
    } else if (!config.empty()) {
      // Use user-specified config file 使用用户指定的配置文件
      std::puts(std::format("Loading configuration from: {}", config).c_str());
      drogon::app().loadConfigFile(config);
    } else {
      // Use default config file 使用默认配置文件
      std::string default_config_path = find_config_file("config.json");
      std::puts(std::format("Loading default configuration from: {}",
                            default_config_path)
                    .c_str());
      drogon::app().loadConfigFile(default_config_path);
    }
  } catch (const std::exception& e) {
    // 捕获配置文件加载异常并输出错误信息
    std::puts(std::format("Error loading configuration: {}", e.what()).c_str());
    return 1;
  }

  try {
    // 初始化服务管理器
    ServiceManager::get_instance().initialize();
  } catch (const std::exception& e) {
    // 捕获服务管理器初始化异常并输出错误信息
    std::puts(std::format("Error initializing service manager: {}", e.what())
                  .c_str());
    return 1;
  }

  if (test_mode) {
    // 测试模式下，在事件循环中输出服务器启动信息和数据库连接信息
    drogon::app().getLoop()->queueInLoop([]() {
      std::cout << std::format(
          "Starting server on 0.0.0.0:5555, DB Connection Info: {}\n",
          drogon::app().getDbClient()->connectionInfo());
    });
  }

  // Create buckets for media files 创建媒体文件桶
  drogon::app().getLoop()->runInLoop([]() {
    drogon::sync_wait([]() -> drogon::Task<void> {
      // 异步确保'media'存储桶存在
      bool bucket_created = co_await ServiceManager::get_instance()
                                .get_s3_service()
                                .ensure_bucket_exists("media");
      if (!bucket_created) {
        LOG_ERROR << "Failed to create or verify 'media' bucket";
        exit(1);    // 创建失败则退出程序
      }
      LOG_INFO << "Media bucket is ready";
      co_return;
    }());
  });

  // 启动Drogon Web服务器，进入事件循环
  drogon::app().run();

  // Cleanup on shutdown 关闭时清理
  std::atexit([]() { ServiceManager::get_instance().shutdown(); });
}
