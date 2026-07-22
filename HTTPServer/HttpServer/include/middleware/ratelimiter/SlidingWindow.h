#pragma once
#include <deque>
#include <mutex>
#include <chrono>

namespace http {
namespace middleware {

class SlidingWindow {
public:
    SlidingWindow(size_t maxRequests, std::chrono::milliseconds windowSize);
    
    // 尝试接受一个请求，返回true表示成功
    bool tryAccept();
    
private:
    void cleanup();  // 清理窗口外的过期请求记录
    
    size_t maxRequests_;
    std::chrono::milliseconds windowSize_;
    std::deque<std::chrono::steady_clock::time_point> requests_; // 请求时间戳队列，按时间顺序存储
    std::mutex mutex_;
};

} // namespace middleware
} // namespace http