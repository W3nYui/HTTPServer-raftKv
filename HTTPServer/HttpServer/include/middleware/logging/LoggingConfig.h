#pragma once
#include <string>
#include <vector>

namespace http {
namespace middleware {

struct LoggingConfig {
    // 日志格式
    enum Format {
        Text,   // 文本格式: "GET /api/users 200 15ms 192.168.1.1"
        JSON    // JSON格式: {"method":"GET","path":"/api/users","status":200,...}
    };
    
    // 输出目标
    enum Output {
        Console,  // 仅控制台输出（使用muduo LOG_INFO）
        File,     // 仅文件输出（异步写入）
        Both      // 同时输出到控制台和文件
    };
    // 初始化默认值
    Format format = Text;
    // Output output = Console;
    Output output = Both;
    std::string logFilePath = "./logs/http_requests.log";
    
    // 排除某些路径不记录（如健康检查端点）
    std::vector<std::string> excludedPaths = {"/health", "/favicon.ico"};
    
    // 是否记录请求体/响应体（默认不记录，避免日志过大）
    bool logRequestBody = false;
    bool logResponseBody = false;
    
    static LoggingConfig defaultConfig() {
        return LoggingConfig();
    }
};

} // namespace middleware
} // namespace http