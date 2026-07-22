#include "../../include/websocket/WebSocketContext.h"

#include <muduo/base/Logging.h>

#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

namespace http
{
namespace websocket
{

// ========== 解析帧 ==========
bool WebSocketContext::parseFrame(muduo::net::Buffer* buf, muduo::Timestamp receiveTime)
{
    if (buf->readableBytes() == 0) return false;

    // 尝试解析一个完整帧
    auto frame = WebSocketFrame::parseFrame(buf->peek(), buf->readableBytes());
    if (!frame.has_value())
    {
        return false; // 数据不完整，等待更多数据
    }

    // 计算帧占用的总字节数
    size_t frameSize = frame->encodeToString().size();

    // 如果帧有掩码，需要计算实际的wire size（因为存储的是解码后的payload）
    // 从原始数据中计算: 2字节头 + 扩展长度 + 掩码key(4) + payload
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(buf->peek());
    size_t offset = 2;
    uint8_t firstLen = raw[1] & 0x7F;
    if (firstLen == 126) offset += 2;
    else if (firstLen == 127) offset += 8;
    bool hasMask = (raw[1] >> 7) & 0x01;
    if (hasMask) offset += 4;
    offset += frame->payload().size();

    // 根据帧类型处理
    switch (frame->opCode())
    {
        case WebSocketOpCode::Close:
            state_ = WebSocketState::Closing;
            break;
        case WebSocketOpCode::Ping:
        case WebSocketOpCode::Pong:
        case WebSocketOpCode::Text:
        case WebSocketOpCode::Binary:
            break;
        default:
            break;
    }

    currentFrame_ = std::move(frame);
    buf->retrieve(offset); // 从缓冲区移除已解析的数据
    return true;
}

// ========== 计算握手 Accept Key ==========
std::string WebSocketContext::computeAcceptKey(const std::string& clientKey)
{
    static const std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = clientKey + GUID;

    // SHA1 哈希
    unsigned char sha1Result[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.size(), sha1Result);

    // Base64 编码
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // 不换行
    BIO_push(b64, mem);
    BIO_write(b64, sha1Result, SHA_DIGEST_LENGTH);
    BIO_flush(b64);

    char* base64Data = nullptr;
    long base64Len = BIO_get_mem_data(mem, &base64Data);
    std::string result(base64Data, base64Len);

    BIO_free_all(b64);
    return result;
}

} // namespace websocket
} // namespace http
