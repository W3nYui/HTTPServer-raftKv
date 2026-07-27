#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <functional>

#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/Logging.h>

#include "WebSocketContext.h"
#include "WebSocketFrame.h"
#include "WebSocketHandler.h"

#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
namespace websocket
{

/**
 * @brief WebSocket 服务器
 * 集成在 HttpServer 内部，管理所有 WebSocket 连接的升级、消息收发与关闭
 *
 * 每个 WebSocket 连接经历以下生命周期：
 *   1. 客户端发送 HTTP Upgrade 请求 (GET /ws + Upgrade: websocket)
 *   2. 路由处理器调用 handleUpgrade() 完成握手
 *   3. 握手成功后，该 TCP 连接接管为 WebSocket 连接
 *   4. 后续数据通过 processFrame() 解析 WebSocket 帧并派发给 handler
 *   5. 任一端发送 Close 帧即进入关闭流程
 */
class WebSocketServer : muduo::noncopyable
{
public:
    using TcpConnectionPtr = muduo::net::TcpConnectionPtr;
    using SendCallback = std::function<void(const TcpConnectionPtr&, const std::string&)>;

    WebSocketServer() = default;

    void setSendCallback(SendCallback callback)
    {
        sendCallback_ = std::move(callback);
    }

    /**
     * @brief 执行 WebSocket 升级握手
     * 验证 Upgrade 头、Sec-WebSocket-Key，计算 Accept Key，
     * 将连接注册为 WebSocket 连接并关联 handler
     *
     * @param req     HTTP 升级请求
     * @param resp    HTTP 响应（填充 101 + Upgrade 头）
     * @param handler 该连接的消息处理器
     * @param conn    TCP 连接
     * @return true 握手成功，false 握手失败
     */
    bool handleUpgrade(const HttpRequest& req,
                       HttpResponse* resp,
                       WebSocketHandlerPtr handler,
                       const TcpConnectionPtr& conn);

    /**
     * @brief 处理 WebSocket 帧数据
     * 在 HttpServer::onMessage() 中被调用，用于解析和分发 WebSocket 帧
     *
     * @param conn TCP 连接
     * @param buf  数据缓冲区
     */
    void processFrame(const TcpConnectionPtr& conn, muduo::net::Buffer* buf);

    /**
     * @brief 发送文本消息到指定连接
     * @param conn    TCP 连接
     * @param message 文本消息
     */
    void sendMessage(const TcpConnectionPtr& conn, const std::string& message);

    /**
     * @brief 主动关闭 WebSocket 连接
     * 发送 Close 帧并清理连接资源
     * @param conn TCP 连接
     */
    void closeConnection(const TcpConnectionPtr& conn);

    /**
     * @brief 判断指定连接是否为 WebSocket 连接
     * @param conn TCP 连接
     */
    bool isWebSocket(const TcpConnectionPtr& conn) const;

    /**
     * @brief 清理指定连接（连接被动断开时调用）
     * @param conn TCP 连接
     */
    void removeConnection(const TcpConnectionPtr& conn);

    /**
     * @brief 获取连接对应的 handler
     * @param conn TCP 连接
     * @return handler 指针，若不存在则返回 nullptr
     */
    WebSocketHandlerPtr getHandler(const TcpConnectionPtr& conn) const;

private:
    /**
     * @brief 验证 WebSocket 升级请求的有效性
     */
    bool validateUpgradeRequest(const HttpRequest& req) const;
    void sendWireData(const TcpConnectionPtr& conn, const std::string& wireData) const;

    // 连接名 -> WebSocket 上下文
    std::unordered_map<std::string, WebSocketContext>  contexts_;
    // 连接名 -> 事件处理器
    std::unordered_map<std::string, WebSocketHandlerPtr> handlers_;
    // 连接名 -> TCP 连接（用于发送消息）
    std::unordered_map<std::string, TcpConnectionPtr>   connections_;
    SendCallback                                         sendCallback_;

    mutable std::mutex mutex_;
};

} // namespace websocket
} // namespace http
