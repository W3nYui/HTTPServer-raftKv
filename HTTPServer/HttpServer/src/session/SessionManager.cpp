#include"../../include/session/SessionManager.h"
#include <iomanip>
#include <iostream>
#include <sstream>

namespace http
{
namespace session
{

// 初始化会话管理器，设置会话存储对象和随机数生成器
SessionManager::SessionManager(std::unique_ptr<SessionStorage> storage)
    : storage_(std::move(storage)) 
    , rng_(std::random_device{}()) // 初始化随机数生成器，用于生成随机的会话ID
{}

// 从请求中获取或创建会话，也就是说，如果请求中包含会话ID，则从存储中加载会话，否则创建一个新的会话
std::shared_ptr<Session> SessionManager::getSession(const HttpRequest& req, HttpResponse* resp)
{   
    std::string sessionId = getSessionIdFromCookie(req);
    
    std::shared_ptr<Session> session;

    if (!sessionId.empty())
    {
        session = storage_->load(sessionId); // 获得会话对象 如果该对象已经过期 会在load中移除
    }

    if (!session || session->isExpired()) // 获得失败或者会话过期
    {
        sessionId = generateSessionId(); // 生成新的会话ID
        session = std::make_shared<Session>(sessionId, this);
        setSessionCookie(sessionId, resp); // 给新的http链接设置会话ID的cookie
    }
    else 
    {
        if (!session->getManager()) {
            session->setManager(this); // 为现有会话设置管理器
        }
    }
    // 更新会话状态 刷新过期时间
    session->refresh();
    storage_->save(session);  // 这里可能有问题，需要确保正确保存会话
    return session;
}

// 生成唯一的会话标识符，确保会话的唯一性和安全性
std::string SessionManager::generateSessionId()
{
    std::stringstream ss;
    std::uniform_int_distribution<> dist(0, 15);

    // 生成32个字符的会话ID，每个字符是一个十六进制数字
    for (int i = 0; i < 32; ++i)
    {
        ss << std::hex << dist(rng_);
    }
    return ss.str();
}

void SessionManager::destroySession(const std::string& sessionId)
{
    storage_->remove(sessionId);
}

// 这里没有从管理类实现 这里从存储类在load时实现 检查是否过期
void SessionManager::cleanExpiredSessions()
{
    // 注意：这个实现依赖于具体的存储实现
    // 对于内存存储，可以在加载时检查是否过期
    // 对于其他存储的实现，可能需要定期清理过期会话
}

/**
 * @brief 从请求中获取会话ID
 * @param req HTTP请求对象
 * @return std::string 会话ID字符串
 * */
std::string SessionManager::getSessionIdFromCookie(const HttpRequest& req)
{
    std::string sessionId;
    std::string cookie = req.getHeader("Cookie"); // 获取http请求头中的cookie

    if (!cookie.empty())
    {
        size_t pos = cookie.find("sessionId=");
        if (pos != std::string::npos)
        {
            pos += 10; // 跳过"sessionId="
            size_t end = cookie.find(';', pos); // 从pos位置开始查找到第一个分号的索引
            if (end != std::string::npos) // 找到分号 返回区间
            {
                sessionId = cookie.substr(pos, end - pos);
            }
            else // 没有找到 返回剩余字符串(可能后面没有其他内容 没有分号了)
            {
                sessionId = cookie.substr(pos);
            }
        }
    }
    
    return sessionId;
}

void SessionManager::setSessionCookie(const std::string& sessionId, HttpResponse* resp)
{
    // 设置会话ID到响应头中，作为Cookie
    std::string cookie = "sessionId=" + sessionId + "; Path=/; HttpOnly";
    resp->addHeader("Set-Cookie", cookie);
}

} // namespace session
} // namespace http