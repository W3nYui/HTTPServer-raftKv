#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>

namespace http {
namespace middleware {

struct RateLimiterConfig {
    // 限流算法类型
    enum Algorithm {
        TokenBucket,      // 令牌桶算法
        SlidingWindow     // 滑动窗口算法
    };
    
    // 限流维度
    enum Dimension {
        Global,           // 全局限流（所有请求共享限制）
        PerIP,            // 按客户端IP限流
        PerRoute,         // 按路由路径限流
        PerUser           // 按用户ID限流
    };
    
    Algorithm algorithm = TokenBucket;
    Dimension dimension = PerIP;
    
    // 令牌桶参数
    double bucketCapacity = 5;    // 桶容量（最大突发请求数）
    double refillRate = 2;         // 令牌补充速率（请求/秒）
    
    // 滑动窗口参数
    size_t maxRequests = 10;       // 窗口内最大请求数
    std::chrono::milliseconds windowSize{30000};  // 窗口大小（默认60秒）
    
    // 路由限流映射（为不同路由设置不同的限流规则）
    // key: 路由路径（支持前缀匹配，如 "/api/"）
    // value: 该路由的限流配置
    std::unordered_map<std::string, std::shared_ptr<RateLimiterConfig>> routeConfigs;
    
    // 获取默认配置
    static RateLimiterConfig defaultConfig() {
        RateLimiterConfig config;
        config.algorithm = TokenBucket;
        config.dimension = PerIP;
        config.bucketCapacity = 5;
        config.refillRate = 2;
        return config;
    }
};

} // namespace middleware
} // namespace http