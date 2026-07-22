#pragma once

#include <string>
#include <vector>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>

#include "WebSocketFrame.h"

namespace http
{
namespace websocket
{

/**
 * @brief WebSocket 连接状态
 */
enum class WebSocketState
{
    Connecting, // 正在握手
    Open,       // 已建立连接
    Closing,    // 正在关闭
    Closed      // 已关闭
};

/**
 * @brief WebSocket 连接上下文
 * 每个 WebSocket 连接持有一个上下文实例，负责帧的解析与状态管理
 */
class WebSocketContext
{
public:
    WebSocketContext()
        : state_(WebSocketState::Connecting)
    {}

    // ========== 状态查询 ==========
    WebSocketState state() const { return state_; }
    void setState(WebSocketState s) { state_ = s; }

    bool isOpen()    const { return state_ == WebSocketState::Open; }
    bool isClosing() const { return state_ == WebSocketState::Closing; }
    bool isClosed()  const { return state_ == WebSocketState::Closed; }

    /**
     * @brief 从 muduo Buffer 中解析 WebSocket 帧
     * @param buf         输入缓冲区
     * @param receiveTime 接收时间戳
     * @return true 解析出一个完整帧，false 数据不完整需等待
     */
    bool parseFrame(muduo::net::Buffer* buf, muduo::Timestamp receiveTime);

    /**
     * @brief 获取最近解析出的帧
     */
    const std::optional<WebSocketFrame>& getFrame() const { return currentFrame_; }

    /**
     * @brief 重置上下文
     */
    void reset()
    {
        state_ = WebSocketState::Connecting;
        currentFrame_.reset();
    }

    /**
     * @brief 生成 WebSocket 握手响应密钥
     * Sec-WebSocket-Accept = base64(sha1(Sec-WebSocket-Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
     * @param clientKey 客户端发送的 Sec-WebSocket-Key
     * @return 计算出的 Sec-WebSocket-Accept 值
     */
    static std::string computeAcceptKey(const std::string& clientKey);

private:
    WebSocketState                    state_;         // 连接状态
    std::optional<WebSocketFrame>     currentFrame_;  // 最近解析的帧
};

} // namespace websocket
} // namespace http
