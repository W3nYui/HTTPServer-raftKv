#include "../../include/websocket/WebSocketServer.h"

#include <muduo/base/Logging.h>

namespace http
{
namespace websocket
{

// ========== WebSocket 握手升级 ==========
bool WebSocketServer::handleUpgrade(const HttpRequest& req,
                                    HttpResponse* resp,
                                    WebSocketHandlerPtr handler,
                                    const TcpConnectionPtr& conn)
{
    // 1. 验证升级请求
    if (!validateUpgradeRequest(req))
    {
        LOG_ERROR << "Invalid WebSocket upgrade request from " << conn->peerAddress().toIpPort();
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setCloseConnection(true);
        return false;
    }

    // 2. 获取客户端 Key 并计算 Accept Key
    std::string clientKey = req.getHeader("Sec-WebSocket-Key");
    std::string acceptKey = WebSocketContext::computeAcceptKey(clientKey);

    // 3. 设置 101 响应
    resp->setStatusCode(HttpResponse::k101SwitchingProtocols);
    resp->setStatusMessage("Switching Protocols");
    resp->setCloseConnection(false);
    resp->addHeader("Upgrade", "websocket");
    resp->addHeader("Connection", "Upgrade");
    resp->addHeader("Sec-WebSocket-Accept", acceptKey);

    // 4. 注册 WebSocket 连接
    std::string connName = conn->name();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        contexts_[connName].setState(WebSocketState::Open);
        handlers_[connName]    = handler;
        connections_[connName] = conn;
    }

    // 5. 通知 handler 连接已建立
    handler->onOpen(conn);

    LOG_INFO << "WebSocket connection upgraded: " << connName;
    return true;
}

// ========== 验证 WebSocket 升级请求 ==========
bool WebSocketServer::validateUpgradeRequest(const HttpRequest& req) const
{
    // 必须是 GET 请求
    if (req.method() != HttpRequest::kGet)
    {
        LOG_ERROR << "WebSocket upgrade must be GET";
        return false;
    }

    // 检查 Upgrade 头
    std::string upgrade = req.getHeader("Upgrade");
    if (upgrade != "websocket")
    {
        LOG_ERROR << "Missing or invalid Upgrade header: " << upgrade;
        return false;
    }

    // 检查 Connection 头（包含 Upgrade）
    std::string connection = req.getHeader("Connection");
    if (connection.find("Upgrade") == std::string::npos)
    {
        LOG_ERROR << "Missing Connection: Upgrade header";
        return false;
    }

    // 检查 Sec-WebSocket-Key
    std::string key = req.getHeader("Sec-WebSocket-Key");
    if (key.empty())
    {
        LOG_ERROR << "Missing Sec-WebSocket-Key header";
        return false;
    }

    // 检查 Sec-WebSocket-Version
    std::string version = req.getHeader("Sec-WebSocket-Version");
    if (version != "13")
    {
        LOG_ERROR << "Unsupported WebSocket version: " << version;
        return false;
    }

    return true;
}

// ========== 处理 WebSocket 帧 ==========
void WebSocketServer::processFrame(const TcpConnectionPtr& conn, muduo::net::Buffer* buf)
{
    std::string connName = conn->name();

    WebSocketContext* ctx = nullptr;
    WebSocketHandlerPtr handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = contexts_.find(connName);
        if (it == contexts_.end()) return;
        ctx = &it->second;

        auto hit = handlers_.find(connName);
        if (hit == handlers_.end()) return;
        handler = hit->second;
    }

    // 循环解析帧（缓冲区中可能有多个帧）
    while (buf->readableBytes() > 0)
    {
        if (!ctx->parseFrame(buf, muduo::Timestamp::now()))
        {
            break; // 数据不完整，等待更多数据
        }

        auto& frame = ctx->getFrame();
        if (!frame.has_value()) break;

        switch (frame->opCode())
        {
            case WebSocketOpCode::Text:
                handler->onMessage(conn, frame->payload());
                break;

            case WebSocketOpCode::Binary:
                // Binary 帧也当作文本处理
                handler->onMessage(conn, frame->payload());
                break;

            case WebSocketOpCode::Ping:
                // 自动回复 Pong
                {
                    auto pongFrame = WebSocketFrame::createFrame(
                        WebSocketOpCode::Pong, frame->payload(), false);
                    sendWireData(conn, pongFrame.encodeToString());
                }
                handler->onPing(conn, frame->payload());
                break;

            case WebSocketOpCode::Pong:
                handler->onPong(conn, frame->payload());
                break;

            case WebSocketOpCode::Close:
                // 发送 Close 帧确认关闭
                {
                    auto closeFrame = WebSocketFrame::createFrame(
                        WebSocketOpCode::Close, "", false);
                    sendWireData(conn, closeFrame.encodeToString());
                }
                handler->onClose(conn);
                removeConnection(conn);
                return;

            default:
                break;
        }
    }
}

// ========== 发送文本消息 ==========
void WebSocketServer::sendMessage(const TcpConnectionPtr& conn, const std::string& message)
{
    auto frame = WebSocketFrame::createFrame(WebSocketOpCode::Text, message, false);
    std::string wireData = frame.encodeToString();
    sendWireData(conn, wireData);
}

// ========== 关闭连接 ==========
void WebSocketServer::closeConnection(const TcpConnectionPtr& conn)
{
    // 发送 Close 帧
    auto closeFrame = WebSocketFrame::createFrame(WebSocketOpCode::Close, "", false);
    std::string wireData = closeFrame.encodeToString();
    sendWireData(conn, wireData);

    // 通知 handler
    WebSocketHandlerPtr handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(conn->name());
        if (it != handlers_.end())
        {
            handler = it->second;
        }
    }
    if (handler) handler->onClose(conn);

    removeConnection(conn);
    conn->shutdown();
}

// ========== 判断是否为 WebSocket 连接 ==========
bool WebSocketServer::isWebSocket(const TcpConnectionPtr& conn) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = contexts_.find(conn->name());
    return it != contexts_.end() && it->second.isOpen();
}

// ========== 清理连接 ==========
void WebSocketServer::removeConnection(const TcpConnectionPtr& conn)
{
    std::string connName = conn->name();
    std::lock_guard<std::mutex> lock(mutex_);
    contexts_.erase(connName);
    handlers_.erase(connName);
    connections_.erase(connName);
    LOG_INFO << "WebSocket connection removed: " << connName;
}

// ========== 获取 handler ==========
WebSocketHandlerPtr WebSocketServer::getHandler(const TcpConnectionPtr& conn) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = handlers_.find(conn->name());
    if (it != handlers_.end()) return it->second;
    return nullptr;
}

void WebSocketServer::sendWireData(const TcpConnectionPtr& conn, const std::string& wireData) const
{
    if (sendCallback_)
    {
        sendCallback_(conn, wireData);
        return;
    }
    conn->send(wireData);
}

} // namespace websocket
} // namespace http
