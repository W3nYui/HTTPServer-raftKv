#pragma once
#include "../Middleware.h"
#include "../../http/HttpRequest.h"
#include "../../http/HttpResponse.h"
#include "RateLimiterConfig.h"
#include "TokenBucket.h"
#include "SlidingWindow.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

namespace http {
namespace middleware {

class RateLimiterMiddleware : public Middleware {
public:
    explicit RateLimiterMiddleware(const RateLimiterConfig& config = RateLimiterConfig::defaultConfig());
    
    void before(HttpRequest& request) override;
    void after(HttpResponse& response) override;

private:
    // 获取限流器的key（根据维度不同，key可能是IP、路由等）
    std::string getLimiterKey(const HttpRequest& request, RateLimiterConfig::Dimension dimension) const;
    
    // 获取或创建限流器实例
    std::shared_ptr<void> getOrCreateLimiter(const std::string& key, const RateLimiterConfig& config);
    
    // 检查是否允许请求通过
    bool isAllowed(const std::string& key, const RateLimiterConfig& config);
    
    // 获取匹配的限流配置
    const RateLimiterConfig& getConfigForRoute(const std::string& path) const;

private:
    RateLimiterConfig config_;
    
    // 限流器实例存储（key -> limiter）
    std::unordered_map<std::string, std::shared_ptr<TokenBucket>> tokenBuckets_;
    std::unordered_map<std::string, std::shared_ptr<SlidingWindow>> slidingWindows_;
    std::mutex mutex_;
};

} // namespace middleware
} // namespace http