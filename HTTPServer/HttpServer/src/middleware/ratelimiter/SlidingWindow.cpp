#include "../../../include/middleware/ratelimiter/SlidingWindow.h"

namespace http {
namespace middleware {

SlidingWindow::SlidingWindow(size_t maxRequests, std::chrono::milliseconds windowSize)
    : maxRequests_(maxRequests)
    , windowSize_(windowSize)
{
}

void SlidingWindow::cleanup() {
    auto now = std::chrono::steady_clock::now();
    auto windowStart = now - windowSize_;
    
    // 移除窗口外的请求记录
    while (!requests_.empty() && requests_.front() < windowStart) {
        requests_.pop_front();
    }
}

bool SlidingWindow::tryAccept() {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup();
    
    if (requests_.size() < maxRequests_) {
        requests_.push_back(std::chrono::steady_clock::now());
        return true;
    }
    return false;
}

} // namespace middleware
} // namespace http