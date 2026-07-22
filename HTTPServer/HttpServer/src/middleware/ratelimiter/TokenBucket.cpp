#include "../../../include/middleware/ratelimiter/TokenBucket.h"
#include <algorithm>

namespace http {
namespace middleware {

TokenBucket::TokenBucket(double capacity, double refillRate)
    : capacity_(capacity)
    , tokens_(capacity)  // 初始时桶是满的
    , refillRate_(refillRate)
    , lastRefill_(std::chrono::steady_clock::now())
{
}

/**
 * @brief 补充令牌桶中的令牌
 * 
 * 每次调用此方法时，根据当前时间与上次补充时间的差值，计算并补充令牌桶中的令牌。
 * 补充的令牌数不会超过桶的容量。
 */
void TokenBucket::refill() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - lastRefill_).count();
    
    // 计算应该补充的令牌数
    double newTokens = elapsed * refillRate_;
    tokens_ = std::min(capacity_, tokens_ + newTokens);
    lastRefill_ = now;
}

/**
 * @brief 尝试消耗令牌桶中的令牌
 * 
 * 如果令牌桶中至少有一个令牌，则消耗一个令牌并返回true。
 * 否则，返回false。
 */
bool TokenBucket::tryConsume() {
    std::lock_guard<std::mutex> lock(mutex_);
    refill();
    
    if (tokens_ >= 1.0) {
        tokens_ -= 1.0;
        return true;
    }
    return false;
}

} // namespace middleware
} // namespace http