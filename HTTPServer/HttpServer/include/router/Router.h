#pragma once
#include <iostream>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <regex>
#include <vector>

#include "RouterHandler.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
namespace router
{

// 选择注册对象式的路由处理器还是注册回调函数式的处理器取决于处理器执行的复杂程度
// 如果是简单的处理可以注册回调函数，否则注册对象式路由处理器(对象中可封装多个相关函数)
// 二者注册其一即可
class Router
{
public:
    using HandlerPtr = std::shared_ptr<RouterHandler>; // 对象式路由处理器指针
    using HandlerCallback = std::function<void(const HttpRequest &, HttpResponse *)>; // 回调式路由处理器函数指针

    // 路由键（请求方法 + URL）
    struct RouteKey
    {
        HttpRequest::Method method; // HTTP 请求方法
        std::string path; // URL 路径

        bool operator==(const RouteKey &other) const
        {
            return method == other.method && path == other.path;
        }
    };

    // 为RouteKey 定义哈希函数
    struct RouteKeyHash
    {
        // size_t operator()(const RouteKey& key) const
        // {
        //     return std::hash<int>{}(static_cast<int>(key.method)) ^
        //            std::hash<std::string>{}(key.path);
        // }
        size_t operator()(const RouteKey &key) const // 为RouteKey 定义哈希函数 组合计算哈希值
        {
            size_t methodHash = std::hash<int>{}(static_cast<int>(key.method));
            size_t pathHash = std::hash<std::string>{}(key.path);
            return methodHash * 31 + pathHash;
        }
    };

    // 一共四种路由注册方式
    // 1. 静态注册 以对象式注册 或 回调式注册 -> 创建hash表 存储对象或回调函数
    // 2. 动态路由注册 以对象式注册 或 回调式注册 -> 创建vector

    // 注册路由处理器 以对象式注册 静态注册
    void registerHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler);

    // 注册回调函数形式的处理器 以回调式注册 静态注册
    void registerCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback);

    // 动态路由用于模式匹配 路径参数提取 动态路由是为了解决动态名问题，如 /user/:id 可以匹配 /user/123 或 /user/453，这样每个用户都有自己的路由且不用注册多个路由
    // 注册动态路由处理器 以对象式注册 动态路由注册
    void addRegexHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
    {
        std::regex pathRegex = convertToRegex(path);
        regexHandlers_.emplace_back(method, pathRegex, handler);
    }

    // 注册动态路由处理函数 以回调式注册 动态路由注册
    void addRegexCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback)
    {
        std::regex pathRegex = convertToRegex(path);
        regexCallbacks_.emplace_back(method, pathRegex, callback);
    }

    // 处理请求
    bool route(const HttpRequest &req, HttpResponse *resp);

private:
    /**
     * @brief 将路径模式转换为正则表达式，支持匹配任意路径参数
     * @param pathPattern 路径模式，如 /user/:id
     * @return std::regex 转换后的正则表达式，如 ^/user/([^/]+)$
     */
    std::regex convertToRegex(const std::string &pathPattern)
    {
        // 将 /:任意非斜杠字符 转换为 /([^/]+) （匹配任意非斜杠字符）
        std::string regexPattern = "^" + std::regex_replace(pathPattern, std::regex(R"(/:([^/]+))"), R"(/([^/]+))") + "$";
        return std::regex(regexPattern);
    }

    // 提取路径参数
    /**
     * @brief 从正则匹配结果中提取路径参数
     * @param match 正则匹配结果
     * @param request 请求对象
     */
    void extractPathParameters(const std::smatch &match, HttpRequest &request)
    {
        // Assuming the first match is the full path, parameters start from index 1
        for (size_t i = 1; i < match.size(); ++i)
        {
            request.setPathParameters("param" + std::to_string(i), match[i].str());
        }
    }

private:
    struct RouteCallbackObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerCallback callback_;
        RouteCallbackObj(HttpRequest::Method method, std::regex pathRegex, const HandlerCallback &callback)
            : method_(method), pathRegex_(pathRegex), callback_(callback) {}
    };

    struct RouteHandlerObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerPtr handler_;
        RouteHandlerObj(HttpRequest::Method method, std::regex pathRegex, HandlerPtr handler)
            : method_(method), pathRegex_(pathRegex), handler_(handler) {}
    };

    std::unordered_map<RouteKey, HandlerPtr, RouteKeyHash>      handlers_;       // 精准匹配
    std::unordered_map<RouteKey, HandlerCallback, RouteKeyHash> callbacks_; // 精准匹配
    std::vector<RouteHandlerObj>                                regexHandlers_;     // 正则匹配
    std::vector<RouteCallbackObj>                               regexCallbacks_;   // 正则匹配
};


} // namespace router
} // namespace http