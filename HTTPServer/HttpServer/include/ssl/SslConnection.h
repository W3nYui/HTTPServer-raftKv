#pragma once
#include "SslContext.h"
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/noncopyable.h>
#include <openssl/ssl.h>
#include <memory>

namespace ssl 
{

using MessageCallback = std::function<void(const std::shared_ptr<muduo::net::TcpConnection>&,
                                         muduo::net::Buffer*,
                                         muduo::Timestamp)>;

class SslConnection : muduo::noncopyable 
{
public:
    using TcpConnectionPtr = std::shared_ptr<muduo::net::TcpConnection>;
    using BufferPtr = muduo::net::Buffer*;
    
    SslConnection(const TcpConnectionPtr& conn, SslContext* ctx);
    ~SslConnection();

    void startHandshake();
    void send(const void* data, size_t len);
    void onRead(const TcpConnectionPtr& conn, BufferPtr buf, muduo::Timestamp time);
    bool isHandshakeCompleted() const { return state_ == SSLState::ESTABLISHED; }
    muduo::net::Buffer* getDecryptedBuffer() { return &decryptedBuffer_; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }

private:
    void handleHandshake();
    void flushWriteBio();

private:
    SSL*                ssl_;
    SslContext*         ctx_;
    TcpConnectionPtr    conn_;
    SSLState            state_;
    BIO*                readBio_;
    BIO*                writeBio_;
    muduo::net::Buffer  decryptedBuffer_;
    MessageCallback     messageCallback_;
    int                 cntHandshake_ = 0;
};

} // namespace ssl
