#pragma once
#include <mutex>
#include <chrono>

namespace http {
namespace middleware {

class TokenBucket {
public:
    TokenBucket(double capacity, double refillRate);
    
    // 尝试消耗一个令牌，返回true表示成功
    bool tryConsume();
    
private:
    void refill();  // 补充令牌
    
    double capacity_;       // 桶容量
    double tokens_;         // 当前令牌数
    double refillRate_;     // 补充速率（每秒）
    std::chrono::steady_clock::time_point lastRefill_;
    std::mutex mutex_;      // 线程安全保护
};

} // namespace middleware
} // namespace http