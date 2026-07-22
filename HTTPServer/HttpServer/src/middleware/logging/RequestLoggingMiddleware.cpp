#include "../../../include/middleware/logging/RequestLoggingMiddleware.h"
#include <muduo/base/Logging.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace http {
namespace middleware {
// 线程局部变量，用于存储每个请求的开始时间和当前请求信息
// 确保每个线程都有自己的独立副本，避免线程安全问题
thread_local muduo::Timestamp RequestLoggingMiddleware::requestStartTime_;
thread_local RequestLoggingMiddleware::RequestInfo RequestLoggingMiddleware::currentRequest_;

RequestLoggingMiddleware::RequestLoggingMiddleware(const LoggingConfig& config)
    : config_(config)
{
    if (config_.output == LoggingConfig::File || config_.output == LoggingConfig::Both) {
        // logFile_.open(config_.logFilePath, std::ios::app);
        // if (!logFile_.is_open()) {
        //     LOG_ERROR << "Failed to open log file: " << config_.logFilePath;
        // }
        // 确保日志文件所在目录存在
        if (!ensureDirectoryExists(config_.logFilePath)) {
            LOG_ERROR << "Failed to create log directory for: " << config_.logFilePath;
            return;
        }
        // 提取日志文件名
        // std::string basename = config_.logFilePath;
        // size_t lastSlash = basename.find_last_of('/');
        // if (lastSlash != std::string::npos) {
        //     basename = basename.substr(lastSlash + 1);
        // }
        
        // 设置日志文件大小为 64MB
        const off_t kRollSize = 64 * 1024 * 1024;
        
        // 创建异步日志对象
        asyncLogging_ = std::make_unique<muduo::AsyncLogging>(config_.logFilePath, kRollSize);
        asyncLogging_->start();
        
        LOG_INFO << "Async logging started for file: " << config_.logFilePath;
    }
}

RequestLoggingMiddleware::~RequestLoggingMiddleware() {
    if (asyncLogging_) {
        asyncLogging_->stop();
        LOG_INFO << "Async logging stopped";
    }
}

void RequestLoggingMiddleware::before(HttpRequest& request) {
    requestStartTime_ = muduo::Timestamp::now();
    
    currentRequest_.method = [&request]() {
        switch (request.method()) {
            case HttpRequest::kGet: return "GET";
            case HttpRequest::kPost: return "POST";
            case HttpRequest::kHead: return "HEAD";
            case HttpRequest::kPut: return "PUT";
            case HttpRequest::kDelete: return "DELETE";
            default: return "UNKNOWN";
        }
    }();
    currentRequest_.path = request.path();
    currentRequest_.startTime = requestStartTime_;
    currentRequest_.clientIP = request.getHeader("X-Forwarded-For");
    currentRequest_.userAgent = request.getHeader("User-Agent");
}

void RequestLoggingMiddleware::after(HttpResponse& response) {
    auto endTime = muduo::Timestamp::now();
    double elapsedMs = timeDifference(endTime, requestStartTime_) * 1000;
    
    currentRequest_.statusCode = static_cast<int>(response.getStatusCode());
    currentRequest_.responseTimeMs = elapsedMs;
    
    if (!shouldExclude(currentRequest_.path)) {
        std::string logLine;
        if (config_.format == LoggingConfig::Text) {
            logLine = formatTextLog(currentRequest_);
        } else {
            logLine = formatJsonLog(currentRequest_);
        }
        writeLog(logLine);
    }
}

std::string RequestLoggingMiddleware::formatTextLog(const RequestInfo& info) const {
    std::ostringstream oss;
    oss << info.method << " " << info.path 
        << " " << info.statusCode 
        << " " << std::fixed << std::setprecision(2) << info.responseTimeMs << "ms"
        << " " << info.clientIP;
    return oss.str();
}

std::string RequestLoggingMiddleware::formatJsonLog(const RequestInfo& info) const {
    // 简单JSON格式化（可使用JsonUtil进行更规范的序列化）
    // std::ostringstream oss;
    // oss << "{"
    //     << "\"method\":\"" << info.method << "\","
    //     << "\"path\":\"" << info.path << "\","
    //     << "\"status\":" << info.statusCode << ","
    //     << "\"response_time_ms\":" << std::fixed << std::setprecision(2) << info.responseTimeMs << ","
    //     << "\"client_ip\":\"" << info.clientIP << "\""
    //     << "}";
    json log;
    log["method"] = info.method;
    log["path"] = info.path;
    log["status"] = info.statusCode;
    log["response_time_ms"] = info.responseTimeMs;
    log["client_ip"] = info.clientIP;

    return log.dump();
}

void RequestLoggingMiddleware::writeLog(const std::string& logLine) {
    // 输出到控制台
    if (config_.output == LoggingConfig::Console || config_.output == LoggingConfig::Both) {
        LOG_INFO << logLine;
    }
    
    // 输出到文件
    if (config_.output == LoggingConfig::File || config_.output == LoggingConfig::Both) {
        writeToFile(logLine);
    }
}

void RequestLoggingMiddleware::writeToFile(const std::string& logLine) {
    if (asyncLogging_) {
        // 异步追加日志，不阻塞当前线程
        std::string logLineCrlf = logLine + "\n";
        asyncLogging_->append(logLineCrlf.c_str(), logLineCrlf.size());
    }
}

bool RequestLoggingMiddleware::shouldExclude(const std::string& path) const {
    return std::find(config_.excludedPaths.begin(), 
                     config_.excludedPaths.end(), 
                     path) != config_.excludedPaths.end();
}

} // namespace middleware
} // namespace http