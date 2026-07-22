#include "../../include/router/Router.h"
#include <muduo/base/Logging.h>

namespace http
{
namespace router
{
/**
 * @brief 注册路由处理器 以对象式注册 静态注册
 * 
 * @param method HTTP方法
 * @param path 路径
 * @param handler 处理器指针
 */
void Router::registerHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
{
    RouteKey key{method, path};
    handlers_[key] = std::move(handler); // 往handlers_中插入该方法以及该路径的处理器
}

/**
 * @brief 注册路由处理器 以回调式注册 静态注册
 * 
 * @param method HTTP方法
 * @param path 路径
 * @param callback 回调函数引用
 */
void Router::registerCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback)
{
    RouteKey key{method, path};
    callbacks_[key] = std::move(callback); // 验证完实际无效 因为回调函数引用是const的 所以不能移动
}

/**
 * @brief 路由请求
 * 依此从：静态对象 -> 静态回调 -> 动态对象 -> 动态回调
 * @param req 请求
 * @param resp 响应
 * @return true 如果找到匹配的处理器或回调函数
 * @return false 如果没有找到匹配的处理器或回调函数
 */
bool Router::route(const HttpRequest &req, HttpResponse *resp)
{
    // 提取请求方法和路径
    RouteKey key{req.method(), req.path()};

    // 查找处理器
    auto handlerIt = handlers_.find(key);
    if (handlerIt != handlers_.end())
    {
        handlerIt->second->handle(req, resp);
        return true;
    }

    // 查找回调函数
    auto callbackIt = callbacks_.find(key);
    if (callbackIt != callbacks_.end())
    {
        callbackIt->second(req, resp);
        return true;
    }

    // 查找动态路由处理器
    for (const auto &[method, pathRegex, handler] : regexHandlers_)
    {
        std::smatch match;
        std::string pathStr(req.path());
        // 如果方法匹配并且动态路由匹配，则执行处理器
        if (method == req.method() && std::regex_match(pathStr, match, pathRegex))
        {
            // Extract path parameters and add them to the request
            HttpRequest newReq(req); // 因为这里需要用这一次所以是可以改的
            extractPathParameters(match, newReq);
            
            handler->handle(newReq, resp);
            return true;
        }
    }

    // 查找动态路由回调函数
    for (const auto &[method, pathRegex, callback] : regexCallbacks_)
    {
        std::smatch match;
        std::string pathStr(req.path());
        // 如果方法匹配并且动态路由匹配，则执行回调函数
        if (method == req.method() && std::regex_match(pathStr, match, pathRegex)) // regex_match中match是匹配结果，包含路径参数以及捕获组
        {
            HttpRequest newReq(req); // 因为这里需要用这一次所以是可以改的
            extractPathParameters(match, newReq); // 定义动态路由的实际参数 写入newReq

            callback(newReq, resp); // 执行回调函数 在回调函数内实现路径替换
            return true;
        }
    }

    return false;
}

} // namespace router
} // namespace http