#pragma once

#include <vector>
#include <memory>
#include "Middleware.h"

namespace http 
{
namespace middleware 
{
/**
请求进来：
  → 中间件1 before → 中间件2 before → Handler
响应回去：
  Handler → 中间件2 after → 中间件1 after → 出去 
  */
class MiddlewareChain 
{
public:
    void addMiddleware(std::shared_ptr<Middleware> middleware);
    void processBefore(HttpRequest& request); // 前向处理 从 begin -> end
    void processAfter(HttpResponse& response); // 后向处理 从 end -> begin

private:
    std::vector<std::shared_ptr<Middleware>> middlewares_; // 管理中间件 形成链 依此顺序执行
};

} // namespace middleware
} // namespace http