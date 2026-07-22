#pragma once

#include <memory>
#include <string>
#include <muduo/net/TcpConnection.h>

namespace http
{
namespace websocket
{

/**
 * @brief WebSocket 事件处理器（抽象基类）
 * 业务层继承此类，实现具体的 WebSocket 消息处理逻辑
 */
class WebSocketHandler
{
public:
    using TcpConnectionPtr = muduo::net::TcpConnectionPtr;

    virtual ~WebSocketHandler() = default;

    /**
     * @brief 连接建立回调
     * @param conn TCP 连接
     */
    virtual void onOpen(const TcpConnectionPtr& conn) = 0;

    /**
     * @brief 收到文本消息回调
     * @param conn    TCP 连接
     * @param message 文本消息内容
     */
    virtual void onMessage(const TcpConnectionPtr& conn, const std::string& message) = 0;

    /**
     * @brief 连接关闭回调
     * @param conn TCP 连接
     */
    virtual void onClose(const TcpConnectionPtr& conn) = 0;

    /**
     * @brief 收到 Ping 帧回调
     * 默认实现自动回复 Pong
     * @param conn    TCP 连接
     * @param payload Ping 负载
     */
    virtual void onPing(const TcpConnectionPtr& conn, const std::string& payload);

    /**
     * @brief 收到 Pong 帧回调
     * @param conn    TCP 连接
     * @param payload Pong 负载
     */
    virtual void onPong(const TcpConnectionPtr& conn, const std::string& payload);
};

using WebSocketHandlerPtr = std::shared_ptr<WebSocketHandler>;

} // namespace websocket
} // namespace http
