#pragma once

#include <muduo/net/TcpServer.h>

namespace http
{
/**
 * @brief HttpResponse类
 * 用于表示http响应
 */
class HttpResponse 
{
public:
    enum HttpStatusCode
    {
        kUnknown,
        k101SwitchingProtocols = 101, // WebSocket 协议升级
        k200Ok = 200, // 成功
        k204NoContent = 204, // 无内容
        k301MovedPermanently = 301, // 永久重定向
        k400BadRequest = 400, // 请求错误
        k401Unauthorized = 401, // 未授权
        k403Forbidden = 403, // 无权访问该网页资源
        k404NotFound = 404, // 未找到该网页资源
        k409Conflict = 409, // 冲突
        k429TooManyRequests = 429, // 过多请求
        k408RequestTimeout = 408, // 请求超时
        k500InternalServerError = 500, // 服务器内部错误
    };

    HttpResponse(bool close = true)
        : statusCode_(kUnknown)
        , closeConnection_(close)
    {}

    void setVersion(std::string version)
    { httpVersion_ = version; }
    void setStatusCode(HttpStatusCode code)
    { statusCode_ = code; }

    HttpStatusCode getStatusCode() const
    { return statusCode_; }

    void setStatusMessage(const std::string message)
    { statusMessage_ = message; }

    void setCloseConnection(bool on)
    { closeConnection_ = on; }

    bool closeConnection() const
    { return closeConnection_; }
    
    void setContentType(const std::string& contentType)
    { addHeader("Content-Type", contentType); }

    void setContentLength(uint64_t length)
    { addHeader("Content-Length", std::to_string(length)); }

    void addHeader(const std::string& key, const std::string& value)
    { headers_[key] = value; }
    
    void setBody(const std::string& body)
    { 
        body_ = body;
        // body_ += "\0";
    }

    void setStatusLine(const std::string& version,
                         HttpStatusCode statusCode,
                         const std::string& statusMessage);

    void setErrorHeader(){}

    void appendToBuffer(muduo::net::Buffer* outputBuf) const;

private:
    std::string                        httpVersion_; 
    HttpStatusCode                     statusCode_;
    std::string                        statusMessage_;
    bool                               closeConnection_;
    std::map<std::string, std::string> headers_;
    std::string                        body_;
    bool                               isFile_;
};

} // namespace http