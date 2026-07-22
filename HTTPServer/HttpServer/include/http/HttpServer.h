#pragma once 

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>

#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "../router/Router.h"
#include "../session/SessionManager.h"
#include "../middleware/MiddlewareChain.h"
#include "../middleware/cors/CorsMiddleware.h"
#include "../middleware/ratelimiter/RateLimiterMiddleware.h"
#include "../middleware/logging/RequestLoggingMiddleware.h"
#include "../ssl/SslConnection.h"
#include "../ssl/SslContext.h"
#include "../websocket/WebSocketServer.h"

class HttpRequest;
class HttpResponse;

namespace http
{
/**
 * @brief HTTP服务器类
 * 集合了整个http服务器的链路
 1. muduo库控制tcp连接
 2. 注册客户端注册(应用层上下文)回调与fd接受回调函数到muduo库中
 3. 实现http数据链路：解析http应用层上下文 -> 中间件 -> 路由匹配 -> 中间件后处理 -> 返回http响应

 */
class HttpServer : muduo::noncopyable
{
public:
    using HttpCallback = std::function<void (const http::HttpRequest&, http::HttpResponse*)>;
    
    // 构造函数
    HttpServer(int port,
               const std::string& name,
               bool useSSL = false, // 默认不使用SSL 是HTTP协议
               muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort);
    // 设置reactor的从线程数
    void setThreadNum(int numThreads)
    {
        server_.setThreadNum(numThreads);
    }

    void start();

    muduo::net::EventLoop* getLoop() const 
    { 
        return server_.getLoop(); 
    }

    void setHttpCallback(const HttpCallback& cb)
    {
        httpCallback_ = cb;
    }

    // 注册静态路由处理器
    void Get(const std::string& path, const HttpCallback& cb)
    {
        router_.registerCallback(HttpRequest::kGet, path, cb);
    }
    
    // 注册静态路由处理器
    void Get(const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.registerHandler(HttpRequest::kGet, path, handler);
    }

    void Post(const std::string& path, const HttpCallback& cb)
    {
        router_.registerCallback(HttpRequest::kPost, path, cb);
    }

    void Post(const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.registerHandler(HttpRequest::kPost, path, handler);
    }

    // 注册动态路由处理器
    void addRoute(HttpRequest::Method method, const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.addRegexHandler(method, path, handler);
    }

    // 注册动态路由处理函数
    void addRoute(HttpRequest::Method method, const std::string& path, const router::Router::HandlerCallback& callback)
    {
        router_.addRegexCallback(method, path, callback);
    }

    // 设置会话管理器
    void setSessionManager(std::unique_ptr<session::SessionManager> manager)
    {
        sessionManager_ = std::move(manager);
    }

    // 获取会话管理器
    session::SessionManager* getSessionManager() const
    {
        return sessionManager_.get();
    }

    // 添加中间件的方法
    void addMiddleware(std::shared_ptr<middleware::Middleware> middleware) 
    {
        middlewareChain_.addMiddleware(middleware);
    }

    void enableSSL(bool enable) 
    {
        useSSL_ = enable;
    }

    void setSslConfig(const ssl::SslConfig& config);

    /**
     * @brief 获取 WebSocket 服务器实例
     * 用于在业务层注册 WebSocket handler 和发送消息
     */
    websocket::WebSocketServer& getWsServer()
    {
        return wsServer_;
    }

    /**
     * @brief 判断指定连接是否已升级为 WebSocket 连接
     */
    bool isWebSocketConnection(const muduo::net::TcpConnectionPtr& conn) const
    {
        return wsServer_.isWebSocket(conn);
    }

private:
    void initialize();

    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                   muduo::net::Buffer* buf,
                   muduo::Timestamp receiveTime);
    void onRequest(const muduo::net::TcpConnectionPtr&, const HttpRequest&);

    void handleRequest(const HttpRequest& req, HttpResponse* resp);
    
private:
    muduo::net::InetAddress                      listenAddr_; // 监听地址
    muduo::net::TcpServer                        server_; // TCP服务器
    muduo::net::EventLoop                        mainLoop_; // 主循环
    HttpCallback                                 httpCallback_; // 回调函数
    router::Router                               router_; // 路由
    std::unique_ptr<session::SessionManager>     sessionManager_; // 会话管理器
    middleware::MiddlewareChain                  middlewareChain_; // 中间件链
    std::unique_ptr<ssl::SslContext>             sslCtx_; // SSL 上下文
    bool                                         useSSL_; // 是否使用 SSL
    // TcpConnectionPtr -> SslConnectionPtr 映射 用于存储每个连接的SSL连接 及其上下文
    std::unordered_map<muduo::net::TcpConnectionPtr, std::unique_ptr<ssl::SslConnection>> sslConns_;
    // WebSocket 服务器 管理所有 WebSocket 连接的升级、消息收发与关闭
    websocket::WebSocketServer                   wsServer_;
}; 

} // namespace http