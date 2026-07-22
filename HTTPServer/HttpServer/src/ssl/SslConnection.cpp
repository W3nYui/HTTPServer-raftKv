#include "../../include/ssl/SslConnection.h"
#include <muduo/base/Logging.h>
#include <openssl/err.h>

namespace ssl
{

// SslConnection构造函数：初始化SSL对象和BIO
SslConnection::SslConnection(const TcpConnectionPtr& conn, SslContext* ctx)
    : ssl_(nullptr)
    , ctx_(ctx)
    , conn_(conn)
    , state_(SSLState::HANDSHAKE)
    , readBio_(nullptr)
    , writeBio_(nullptr)
    , messageCallback_(nullptr)
{
    if (!ctx || !conn) {
        LOG_ERROR << "SslConnection constructor: ctx or conn is null";
        return;
    }
    
    // 创建SSL对象，基于SslContext的配置初始化一个SSL对象
    ssl_ = SSL_new(ctx_->getNativeHandle());
    if (!ssl_) {
        LOG_ERROR << "Failed to create SSL object";
        return;
    }

    // 创建内存BIO用于读写操作
    // readBio_: 接收网络数据，供SSL_read解密
    // writeBio_: 接收SSL_write加密后的数据，需要发送到网络
    readBio_ = BIO_new(BIO_s_mem());
    writeBio_ = BIO_new(BIO_s_mem());
    
    if (!readBio_ || !writeBio_) {
        LOG_ERROR << "Failed to create BIO objects";
        SSL_free(ssl_);
        ssl_ = nullptr;
        return;
    }

    // 将BIO绑定到SSL对象
    SSL_set_bio(ssl_, readBio_, writeBio_);
    // 设置为服务器模式（接受SSL连接）
    SSL_set_accept_state(ssl_);
    
    // 设置SSL模式：允许部分写入和移动写缓冲区
    SSL_set_mode(ssl_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);
}

SslConnection::~SslConnection() 
{
    if (ssl_) {
        SSL_free(ssl_);
    }
}

// 开始SSL握手过程
void SslConnection::startHandshake() 
{
    SSL_set_accept_state(ssl_); // 设置为服务器模式（接受SSL连接）
    // handleHandshake(); // 由于客户端还没有发送数据，所以这里不执行握手，等客户端发送数据后再执行
}

// 处理TCP连接上的读事件
// 这是SSL连接的核心处理函数，负责：
// 1. 握手阶段：处理握手数据
// 2. 数据传输阶段：解密接收到的数据
void SslConnection::onRead(const TcpConnectionPtr& conn, BufferPtr buf, 
                         muduo::Timestamp time) 
{
    if (!ssl_) {
        LOG_ERROR << "SSL object is null";
        return;
    }
    
    // 握手阶段处理
    if (state_ == SSLState::HANDSHAKE) {
        // 将TCP接收到的数据写入readBio，供SSL_do_handshake使用 因为ssl握手是将客户端发送的数据加密后返回
        if (buf->readableBytes() > 0) {
            BIO_write(readBio_, buf->peek(), buf->readableBytes());
            buf->retrieve(buf->readableBytes());
        }
        handleHandshake();
        
        // 如果握手还未完成，等待更多数据
        if (state_ != SSLState::ESTABLISHED) {
            return;
        }
    }
    
    // 数据传输阶段：解密接收到的数据
    if (state_ == SSLState::ESTABLISHED) {
        // 将加密数据写入readBio
        if (buf->readableBytes() > 0) {
            BIO_write(readBio_, buf->peek(), buf->readableBytes());
            buf->retrieve(buf->readableBytes());
        }
        
        // 循环读取所有可用的解密数据
        char decryptedData[8192];
        int ret;
        while ((ret = SSL_read(ssl_, decryptedData, sizeof(decryptedData))) > 0) {
            decryptedBuffer_.append(decryptedData, ret);
        }
        
        // 处理SSL_read错误
        if (ret < 0) {
            int err = SSL_get_error(ssl_, ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                // 正常情况，等待更多数据
            } else {
                LOG_ERROR << "SSL_read failed with error: " << err;
                unsigned long sslErr = ERR_get_error();
                if (sslErr != 0) {
                    char errBuf[256];
                    ERR_error_string_n(sslErr, errBuf, sizeof(errBuf));
                    LOG_ERROR << "SSL error details: " << errBuf;
                }
                conn_->shutdown();
                return;
            }
        }
    }
}

// 通过SSL加密发送数据
// 数据流向：明文数据 -> SSL_write加密 -> writeBio -> TCP发送
void SslConnection::send(const void* data, size_t len) 
{
    if (state_ != SSLState::ESTABLISHED) {
        LOG_ERROR << "Cannot send data before SSL handshake is complete";
        return;
    }
    
    // SSL_write将明文数据加密，加密后的数据存入writeBio
    int written = SSL_write(ssl_, data, len);
    if (written <= 0) {
        int err = SSL_get_error(ssl_, written);
        LOG_ERROR << "SSL_write failed: " << err;
        return;
    }
    
    // 将writeBio中的加密数据发送到TCP连接
    flushWriteBio();
}

// 执行SSL握手
// 调用SSL_do_handshake处理握手数据
// 握手成功后状态变为ESTABLISHED
void SslConnection::handleHandshake() 
{
    // 执行一次SSL握手
    int ret = SSL_do_handshake(ssl_);
    cntHandshake_++;
    LOG_INFO << "SSL handshake cur attempt " << cntHandshake_;
    
    // 将握手响应数据从writeBio发送到TCP
    flushWriteBio();
    // 握手成功后状态变为ESTABLISHED
    if (ret == 1) {
        state_ = SSLState::ESTABLISHED;
        LOG_INFO << "SSL handshake completed successfully";
        LOG_INFO << "Using cipher: " << SSL_get_cipher(ssl_);
        LOG_INFO << "Protocol version: " << SSL_get_version(ssl_);
        
        // ssl的数据是在httpserver的onMessage中利用decryptedBuffer_进行获取的 不需要回调 可以改进成回调 直接注入httpserver
        // if (!messageCallback_) {
        //     LOG_WARN << "No message callback set after SSL handshake";
        // }
        return;
    }
    
    // 处理握手错误
    int err = SSL_get_error(ssl_, ret);
    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
        unsigned long sslErr = ERR_get_error();
        if (sslErr != 0) {
            char errBuf[256];
            ERR_error_string_n(sslErr, errBuf, sizeof(errBuf));
            LOG_ERROR << "SSL handshake failed: " << errBuf;
        } else {
            LOG_ERROR << "SSL handshake failed with error code: " << err;
        }
        conn_->shutdown();
    }
}

// 将writeBio中的加密数据发送到TCP连接
// 这是BIO架构的关键：SSL_write只加密数据到writeBio，需要手动flush到网络
void SslConnection::flushWriteBio()
{
    char buf[4096];
    int pending;
    // 循环读取writeBio中所有待发送的数据
    while ((pending = BIO_pending(writeBio_)) > 0) {
        int bytes = BIO_read(writeBio_, buf, 
                           std::min(pending, static_cast<int>(sizeof(buf))));
        if (bytes > 0) {
            conn_->send(buf, bytes);
        }
    }
}

} // namespace ssl
