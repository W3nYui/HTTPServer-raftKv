#pragma once

#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http 
{
namespace middleware 
{
// 纯虚函数 作为中间件的基类 定义接口
class Middleware 
{
public:
    virtual ~Middleware() = default;
    
    // 请求(handler)前处理
    virtual void before(HttpRequest& request) = 0;
    
    // 响应(handler)后处理
    virtual void after(HttpResponse& response) = 0;
    
    // 设置下一个中间件
    void setNext(std::shared_ptr<Middleware> next) 
    {
        nextMiddleware_ = next;
    }

protected:
    std::shared_ptr<Middleware> nextMiddleware_;
};

} // namespace middleware
} // namespace http