#pragma once
#include "../Middleware.h"
#include "../../http/HttpRequest.h"
#include "../../http/HttpResponse.h"
#include "LoggingConfig.h"
#include "../../utils/JsonUtil.h"

#include <muduo/base/Timestamp.h>
#include <muduo/base/AsyncLogging.h>
#include <fstream>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

namespace http {
namespace middleware {

class RequestLoggingMiddleware : public Middleware {
public:
    explicit RequestLoggingMiddleware(const LoggingConfig& config = LoggingConfig::defaultConfig());
    // 也可以写成委托构造的形式
    /*
    RequestLoggingMiddleware() : RequestLoggingMiddleware(LoggingConfig::defaultConfig()) {}
    explicit RequestLoggingMiddleware (const LoggingConfig& config); // 这样委托实现默认构造函数
    */
    ~RequestLoggingMiddleware();
    
    void before(HttpRequest& request) override;
    void after(HttpResponse& response) override;

private:
    struct RequestInfo {
        std::string method;
        std::string path;
        std::string query;
        int statusCode = 0;
        double responseTimeMs = 0;
        std::string clientIP;
        std::string userAgent;
        muduo::Timestamp startTime;
    };

    // 日志格式化
    std::string formatTextLog(const RequestInfo& info) const;
    std::string formatJsonLog(const RequestInfo& info) const;
    
    // 日志输出
    void writeLog(const std::string& logLine);
    void writeToFile(const std::string& logLine);
    
    // 检查路径是否应该被排除
    bool shouldExclude(const std::string& path) const;
    
    
    // 存储请求开始时间（通过线程局部变量或请求上下文）
    thread_local static muduo::Timestamp requestStartTime_;
    thread_local static RequestInfo currentRequest_;

private:
    LoggingConfig config_;
    // std::ofstream logFile_;  这里改进成muduo的AsyncFileAppender异步写入文件
    // std::mutex fileMutex_;
    std::unique_ptr<muduo::AsyncLogging> asyncLogging_;

    bool ensureDirectoryExists (const std::string& path) {
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash == std::string::npos) { // 没有目录部分，直接返回true
            return true;
        }
        std::string dirPath = path.substr(0, lastSlash);
        struct stat st;
        if (stat(dirPath.c_str(), &st) == 0) {
            return S_ISDIR(st.st_mode); // 检查是否为目录
        }
        std::string::size_type pos = 0;
        // 递归创建目录
        do {
            pos = dirPath.find_first_of('/', pos + 1);
            std::string subPath = dirPath.substr(0, pos);
            
            if (!subPath.empty()) {
                struct stat st2;
                if (stat(subPath.c_str(), &st2) != 0) {
                    // 目录不存在，创建它
                    if (mkdir(subPath.c_str(), 0755) != 0) {
                        return false; // 创建失败
                    }
                }
            }
        } while (pos != std::string::npos);
        
        return true;
    }
};

} // namespace middleware
} // namespace http