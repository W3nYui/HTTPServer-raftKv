#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace http
{
namespace websocket
{

/**
 * @brief WebSocket 操作码
 */
enum class WebSocketOpCode : uint8_t
{
    Continuation = 0x0, // 延续帧
    Text         = 0x1, // 文本帧
    Binary       = 0x2, // 二进制帧
    Close        = 0x8, // 关闭连接
    Ping         = 0x9, // 心跳Ping
    Pong         = 0xA  // 心跳Pong
};

/**
 * @brief WebSocket 帧结构
 * 符合 RFC 6455 规定的帧格式
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-------+-+-------------+-------------------------------+
 * |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 * |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 * |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 * | |1|2|3|       |K|             |                               |
 * +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - -+
 * |     Masking-key (if MASK set)  |       Payload Data            |
 * +-------------------------------+ - - - - - - - - - - - - - - -+
 * :                     Payload Data continued ...                :
 * + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -+
 */
class WebSocketFrame
{
public:
    WebSocketFrame()
        : fin_(true)
        , opCode_(WebSocketOpCode::Text)
        , mask_(false)
        , payloadLen_(0)
        , maskingKey_(0)
    {}

    // ========== 属性获取 ==========
    bool             fin()        const { return fin_; }
    WebSocketOpCode  opCode()     const { return opCode_; }
    bool             mask()       const { return mask_; }
    uint64_t         payloadLen() const { return payloadLen_; }
    uint32_t         maskingKey() const { return maskingKey_; }
    const std::string& payload()  const { return payload_; }

    // ========== 属性设置 ==========
    void setFin(bool f)               { fin_ = f; }
    void setOpCode(WebSocketOpCode o) { opCode_ = o; }
    void setMask(bool m)              { mask_ = m; }
    void setPayloadLen(uint64_t len)  { payloadLen_ = len; }
    void setMaskingKey(uint32_t key)  { maskingKey_ = key; }
    void setPayload(const std::string& p) { payload_ = p; }
    void setPayload(std::string&& p)       { payload_ = std::move(p); }

    /**
     * @brief 从原始数据解析 WebSocket 帧
     * @param data   原始数据指针
     * @param len    数据长度
     * @param clientToServer 是否为客户端发往服务端（客户端帧必定带mask）
     * @return 解析后的帧对象，解析失败返回 std::nullopt
     */
    static std::optional<WebSocketFrame> parseFrame(const char* data, size_t len);

    /**
     * @brief 创建 WebSocket 帧
     * @param op      操作码
     * @param payload 负载数据
     * @param mask    是否掩码（服务端发往客户端不需要掩码）
     * @return 构建好的帧对象
     */
    static WebSocketFrame createFrame(WebSocketOpCode op,
                                      const std::string& payload,
                                      bool mask = false);

    /**
     * @brief 将帧编码为 wire format 字符串
     * @return 编码后的字节序列
     */
    std::string encodeToString() const;

private:
    bool             fin_;         // 是否为最后一帧
    WebSocketOpCode  opCode_;      // 操作码
    bool             mask_;        // 是否掩码
    uint64_t         payloadLen_;  // 负载长度
    uint32_t         maskingKey_;  // 掩码密钥（客户端帧使用）
    std::string      payload_;     // 负载数据
};

} // namespace websocket
} // namespace http
