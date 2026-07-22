#include "../../../include/middleware/ratelimiter/RateLimiterMiddleware.h"
#include "../../../include/http/HttpResponse.h"
#include <muduo/base/Logging.h>

namespace http {
namespace middleware {

RateLimiterMiddleware::RateLimiterMiddleware(const RateLimiterConfig& config)
    : config_(config)
{
}
/**
 * @brief 处理请求前的限流检查
 * 
 * @param request 当前请求
 */
void RateLimiterMiddleware::before(HttpRequest& request) {
    // 根据请求路径获取限流配置 不同路径的内容可以承受的请求数不同
    const RateLimiterConfig& requestConfig = getConfigForRoute(request.path());
    std::string key = getLimiterKey(request, requestConfig.dimension);
    // 进行IP限流判断 是否超过IP的请求数限制
    if (!isAllowed(key, requestConfig)) {
        LOG_ERROR << "Rate limit exceeded for key: " << key << " now limited";
        HttpResponse response;
        response.setVersion(request.getVersion());
        response.setStatusCode(HttpResponse::HttpStatusCode::k429TooManyRequests);
        response.setStatusMessage("Too Many Requests");
        response.setContentType("text/plain");
        response.setBody("Too Many Requests. Please try again later.");
        response.addHeader("Retry-After", "60");  // 建议60秒后重试
        throw response;  // 抛出响应，中断请求处理
    }
}

void RateLimiterMiddleware::after(HttpResponse& response) {
    // 后处理不需要特殊操作
}

std::string RateLimiterMiddleware::getLimiterKey(const HttpRequest& request, RateLimiterConfig::Dimension dimension) const {
    switch (dimension) {
        case RateLimiterConfig::Global:
            return "__global__";
        case RateLimiterConfig::PerIP:
            return "ip:" + request.getClientIP();
        case RateLimiterConfig::PerRoute:
            return "route:" + request.path();
        case RateLimiterConfig::PerUser:
            // 需要从Session获取用户ID，可在认证中间件之后使用
            return "user:anonymous";  // 简化处理
        default:
            return "__global__";
    }
}

std::shared_ptr<void> RateLimiterMiddleware::getOrCreateLimiter(const std::string& key, const RateLimiterConfig& config) {
    if (config.algorithm == RateLimiterConfig::TokenBucket) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (tokenBuckets_.find(key) == tokenBuckets_.end()) {
            tokenBuckets_[key] = std::make_shared<TokenBucket>(
                config.bucketCapacity, 
                config.refillRate
            );
        }
        return tokenBuckets_[key];
    } else {
        std::lock_guard<std::mutex> lock(mutex_);
        if (slidingWindows_.find(key) == slidingWindows_.end()) {
            slidingWindows_[key] = std::make_shared<SlidingWindow>(
                config.maxRequests,
                config.windowSize
            );
        }
        return slidingWindows_[key];
    }
}

bool RateLimiterMiddleware::isAllowed(const std::string& key, const RateLimiterConfig& config) {
    auto limiter = getOrCreateLimiter(key, config); // 返回对应的限流器实例
    
    if (config.algorithm == RateLimiterConfig::TokenBucket) {
        auto bucket = std::static_pointer_cast<TokenBucket>(limiter);
        return bucket->tryConsume();
    } else {
        auto window = std::static_pointer_cast<SlidingWindow>(limiter);
        return window->tryAccept();
    }
}

const RateLimiterConfig& RateLimiterMiddleware::getConfigForRoute(const std::string& path) const {
    for (const auto& pair : config_.routeConfigs) {
        const auto& routePath = pair.first;
        const auto& routeConfigPtr = pair.second;
        if (path.find(routePath) == 0) {
            return *routeConfigPtr;
        }
    }
    return config_;
}

} // namespace middleware
} // namespace http
