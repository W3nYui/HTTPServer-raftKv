#include "../../include/websocket/WebSocketFrame.h"

#include <arpa/inet.h>
#include <cstring>
#include <endian.h>
#include <optional>
#include <random>

namespace http
{
namespace websocket
{

// ========== 解析帧 ==========
std::optional<WebSocketFrame> WebSocketFrame::parseFrame(const char* data, size_t len)
{
    // 至少需要2字节（FIN+opcode + MASK+payload_len）
    if (len < 2) return std::nullopt;

    WebSocketFrame frame;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);

    // 字节0: FIN(1bit) + RSV(3bit) + OPCODE(4bit)
    frame.fin_    = (ptr[0] >> 7) & 0x01;
    frame.opCode_ = static_cast<WebSocketOpCode>(ptr[0] & 0x0F);

    // 字节1: MASK(1bit) + PayloadLen(7bit)
    frame.mask_ = (ptr[1] >> 7) & 0x01;
    uint64_t payloadLen = ptr[1] & 0x7F;
    size_t offset = 2;

    // 扩展载荷长度
    if (payloadLen == 126)
    {
        if (len < 4) return std::nullopt;
        uint16_t extLen;
        std::memcpy(&extLen, ptr + offset, 2);
        payloadLen = ntohs(extLen);
        offset += 2;
    }
    else if (payloadLen == 127)
    {
        if (len < 10) return std::nullopt;
        uint64_t extLen;
        std::memcpy(&extLen, ptr + offset, 8);
        // 网络字节序转主机字节序（64位）
        payloadLen = ((uint64_t)ntohl(extLen >> 32)) |
                    (((uint64_t)ntohl(extLen & 0xFFFFFFFF)) << 32);
        // 简化处理：使用 be64toh 更直接
        payloadLen = be64toh(extLen);
        offset += 8;
    }

    // 掩码密钥
    uint32_t maskKey = 0;
    if (frame.mask_)
    {
        if (len < offset + 4) return std::nullopt;
        std::memcpy(&maskKey, ptr + offset, 4);
        offset += 4;
    }

    // 校验数据长度
    if (len < offset + payloadLen) return std::nullopt;

    // 提取负载数据
    std::string payload(reinterpret_cast<const char*>(ptr + offset), payloadLen);

    // 如果有掩码，则解码
    if (frame.mask_)
    {
        uint8_t* maskBytes = reinterpret_cast<uint8_t*>(&maskKey);
        for (size_t i = 0; i < payload.size(); ++i)
        {
            payload[i] ^= maskBytes[i % 4];
        }
    }

    frame.payloadLen_  = payloadLen;
    frame.maskingKey_  = maskKey;
    frame.payload_     = std::move(payload);
    return frame;
}

// ========== 创建帧 ==========
WebSocketFrame WebSocketFrame::createFrame(WebSocketOpCode op,
                                           const std::string& payload,
                                           bool mask)
{
    WebSocketFrame frame;
    frame.setFin(true);
    frame.setOpCode(op);
    frame.setMask(mask);
    frame.setPayloadLen(payload.size());

    if (mask)
    {
        // 生成随机掩码密钥
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist;
        frame.setMaskingKey(dist(gen));

        // 掩码加密负载
        std::string maskedPayload = payload;
        uint8_t* keyBytes = reinterpret_cast<uint8_t*>(&frame.maskingKey_);
        for (size_t i = 0; i < maskedPayload.size(); ++i)
        {
            maskedPayload[i] ^= keyBytes[i % 4];
        }
        frame.setPayload(std::move(maskedPayload));
    }
    else
    {
        frame.setPayload(payload);
    }

    return frame;
}

// ========== 编码为 wire format ==========
std::string WebSocketFrame::encodeToString() const
{
    std::string result;
    result.reserve(10 + payload_.size()); // 预留头部空间

    // 字节0: FIN(1) + RSV(000) + OPCODE(4)
    uint8_t byte0 = (fin_ ? 0x80 : 0x00) | (static_cast<uint8_t>(opCode_) & 0x0F);
    result.push_back(static_cast<char>(byte0));

    // 字节1: MASK(1) + PayloadLen(7)
    uint64_t len = payload_.size();
    if (len <= 125)
    {
        uint8_t byte1 = (mask_ ? 0x80 : 0x00) | static_cast<uint8_t>(len);
        result.push_back(static_cast<char>(byte1));
    }
    else if (len <= 0xFFFF)
    {
        uint8_t byte1 = (mask_ ? 0x80 : 0x00) | 126;
        result.push_back(static_cast<char>(byte1));
        uint16_t extLen = htons(static_cast<uint16_t>(len));
        result.append(reinterpret_cast<const char*>(&extLen), 2);
    }
    else
    {
        uint8_t byte1 = (mask_ ? 0x80 : 0x00) | 127;
        result.push_back(static_cast<char>(byte1));
        uint64_t extLen = htobe64(len);
        result.append(reinterpret_cast<const char*>(&extLen), 8);
    }

    // 掩码密钥
    if (mask_)
    {
        result.append(reinterpret_cast<const char*>(&maskingKey_), 4);
    }

    // 负载数据
    result.append(payload_);

    return result;
}

} // namespace websocket
} // namespace http
