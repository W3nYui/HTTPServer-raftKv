#include "../../include/http/HttpServer.h"
#include "../../include/http/HttpRequest.h"

#include <any>
#include <functional>
#include <memory>

namespace http
{

// 默认http回应函数
void defaultHttpCallback(const HttpRequest &, HttpResponse *resp)
{
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
}

HttpServer::HttpServer(int port,
                       const std::string &name,
                       bool useSSL,
                       muduo::net::TcpServer::Option option)
    : listenAddr_(port) // 注册监听端口
    , server_(&mainLoop_, listenAddr_, name, option) // 注册muduo库的tcp服务器：reactor模式
    , useSSL_(useSSL)
    , httpCallback_(std::bind(&HttpServer::handleRequest, this, std::placeholders::_1, std::placeholders::_2)) // 注册路由匹配函数的回调函数
{
    initialize();
}

// 服务器运行函数
void HttpServer::start()
{
    LOG_WARN << "HttpServer[" << server_.name() << "] starts listening on" << server_.ipPort();
    server_.start(); // 启动tcp服务器的主循环监听客户端连接
    mainLoop_.loop(); // 进入主循环 等待客户端连接
    LOG_WARN << "HttpServer[" << server_.name() << "] stops listening on" << server_.ipPort();
}

void HttpServer::initialize()
{
    // 设置回调函数
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1)); // 客户端连接回调函数
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this,
                  std::placeholders::_1,
                  std::placeholders::_2,
                  std::placeholders::_3)); // 客户端消息回调函数
}

void HttpServer::setSslConfig(const ssl::SslConfig& config)
{
    if (useSSL_)
    {
        sslCtx_ = std::make_unique<ssl::SslContext>(config); // 将config注册到sslCtx_中
        if (!sslCtx_->initialize()) // 初始化ssl上下文
        {
            LOG_ERROR << "Failed to initialize SSL context";
            abort();
        }
    }
}

void HttpServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
    if (conn->connected())
    {
        if (useSSL_) // 创建一个ssl连接
        {   // 对unique_ptr使用get 获得的是sslCtx_的指针 但不获取所有权 当上层(httpserver)的指针销毁时 其下层的ssl连接也会被销毁
            auto sslConn = std::make_unique<ssl::SslConnection>(conn, sslCtx_.get()); // 将TCP连接与ssl上下文传入ssl连接中
            sslConns_[conn] = std::move(sslConn);
            sslConns_[conn]->startHandshake(); // 设定该conn对应的ssl连接模式
        }
        conn->setContext(HttpContext()); // 设置muduo库 在其中传入一个HttpContext对象 用来管理http数据解析的状态机
    }
    else
    {
        // 清理 WebSocket 连接
        if (wsServer_.isWebSocket(conn))
        {
            wsServer_.removeConnection(conn);
        }
        if (useSSL_)
        {
            sslConns_.erase(conn);
        }
    }
}
/**
 * @brief 客户端消息回调函数
 当客户端数据到达，进行数据解析 解析应用层http协议
 * @param conn 客户端连接
 * @param buf 客户端发送的消息缓冲区
 * @param receiveTime 客户端发送消息的时间
 */
void HttpServer::onMessage(const muduo::net::TcpConnectionPtr &conn,
                           muduo::net::Buffer *buf,
                           muduo::Timestamp receiveTime)
{
    // 如果该连接已升级为 WebSocket，则走 WebSocket 帧处理流程
    if (wsServer_.isWebSocket(conn))
    {
        try
        {
            wsServer_.processFrame(conn, buf);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "WebSocket processFrame error: " << e.what();
            wsServer_.removeConnection(conn);
            conn->shutdown();
        }
        return;
    }

    try
    {
        // 这层判断只是代表是否支持ssl
        if (useSSL_)
        {
            // 1.查找对应的SSL连接
            auto it = sslConns_.find(conn);
            if (it != sslConns_.end())
            {
                // 2. SSL连接处理数据（握手或解密）
                it->second->onRead(conn, buf, receiveTime);

                // 3. 如果 SSL 握手还未完成，直接返回
                if (!it->second->isHandshakeCompleted())
                {
                    return;
                }

                // 4. 从SSL连接的解密缓冲区获取数据
                muduo::net::Buffer* decryptedBuf = it->second->getDecryptedBuffer();
                if (decryptedBuf->readableBytes() == 0)
                    return; // 没有解密后的数据

                // 5. 使用解密后的数据进行HTTP处理
                buf = decryptedBuf; // 将 buf 指向解密后的数据
            }
        }
        // HttpContext对象用于解析出buf中的请求报文，并把报文的关键信息封装到HttpRequest对象中
        // 这里获取的是conn内的HttpContext对象 因此用指针来操作 其中管理了一个http的上下文状态
        HttpContext *context = boost::any_cast<HttpContext>(conn->getMutableContext());
        if (!context->parseRequest(buf, receiveTime)) // 解析一个http请求 并存储到httpcontext中的request对象中
        {
            // 如果解析http报文过程中出错
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->shutdown();
        }
        // 如果buf缓冲区中解析出一个完整的数据包才封装响应报文 完全解析才会封装 否则等待下一次的数据到达 进行封装
        if (context->gotAll())
        {
            onRequest(conn, context->request()); // 执行响应函数
            context->reset(); // 重置http请求的上下文状态 以及保存的http请求数据
            
            // 清空SSL解密缓冲区，避免数据累积
            if (useSSL_)
            {
                auto it = sslConns_.find(conn);
                if (it != sslConns_.end())
                {
                    it->second->getDecryptedBuffer()->retrieveAll();
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        // 捕获异常，返回错误信息
        LOG_ERROR << "Exception in onMessage: " << e.what();
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }
}
/**
 * @brief 客户端请求回调函数
 * 当客户端发送http请求时，调用该函数
 * @param conn 客户端连接
 * @param req 客户端发送的http请求
 */
void HttpServer::onRequest(const muduo::net::TcpConnectionPtr &conn, const HttpRequest &req)
{
    HttpRequest mutableReq = req; // 设置客户端IP地址
    mutableReq.setClientIP(conn->peerAddress().toIp());
    mutableReq.setConnectionPtr(&conn); // 设置TCP连接指针 用于WebSocket升级

    const std::string &connection = mutableReq.getHeader("Connection");
    bool checkClose = ((connection == "close") || // 如果有Connection头 且值为close 则关闭连接
                  (mutableReq.getVersion() == "HTTP/1.0" && connection != "Keep-Alive")); // 如果不是长连接版本 则关闭连接
    HttpResponse response(checkClose); // 设置响应

    httpCallback_(mutableReq, &response);

    muduo::net::Buffer buf;
    response.appendToBuffer(&buf);
    LOG_INFO << "Sending response:\n" << buf.toStringPiece().as_string();

    // ssl加密发送响应 或 普通发送响应
    if (useSSL_)
    {
        auto it = sslConns_.find(conn);
        if (it != sslConns_.end())
        {
            it->second->send(buf.peek(), buf.readableBytes());
        }
        else
        {
            conn->send(&buf);
        }
    }
    else
    {
        conn->send(&buf);
    }

    if (response.closeConnection())
    {
        conn->shutdown();
    }
}

// 执行请求对应的路由处理函数
void HttpServer::handleRequest(const HttpRequest &req, HttpResponse *resp)
{
    try
    {
        // 处理请求前的中间件
        HttpRequest mutableReq = req;
        middlewareChain_.processBefore(mutableReq);

        // 路由处理
        if (!router_.route(mutableReq, resp))
        {
            LOG_INFO << "请求的url:" << req.method() << " " << req.path();
            LOG_INFO << "未找到路由,返回404";
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
        }

        // 处理响应后的中间件
        middlewareChain_.processAfter(*resp);
    }
    catch (const HttpResponse& res) // 抛出响应对象 则直接返回响应
    {
        // 处理中间件抛出的响应（如CORS预检请求）
        *resp = res;
    }
    catch (const std::exception& e) 
    {
        // 错误处理
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setBody(e.what());
    }
}

} // namespace http