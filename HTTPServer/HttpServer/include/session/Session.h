#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>

namespace http
{

namespace session
{

class SessionManager;

/**
 * @brief 会话类 用于存储用户登录状态、游戏状态、访问权限等
 * */
class SessionManager;
class Session : public std::enable_shared_from_this<Session> // 继承enable_shared_from_this类 用于获取当前对象的shared_ptr
{
public:
    Session(const std::string& sessionId, SessionManager* sessionManager = nullptr, int maxAge = 3600); // 默认1小时过期
    
    const std::string& getId() const 
    { return sessionId_; }

    bool isExpired() const;
    void refresh(); // 刷新过期时间
    // 指定上层会话管理器指针
    void setManager(SessionManager* sessionManager) 
    { sessionManager_ = sessionManager; }

    SessionManager* getManager() const 
    { return sessionManager_; }

    // 数据存取
    void setValue(const std::string&key, const std::string&value);
    std::string getValue(const std::string&key) const;
    void remove(const std::string&key);
    void clear();
private:
    std::string                                  sessionId_; // session 会话id
    std::unordered_map<std::string, std::string> data_; // session 数据
    std::chrono::system_clock::time_point        expiryTime_; // 过期时间
    int                                          maxAge_; // 有效时间（秒） -> expiryTime_ = now() + maxAge_;
    SessionManager*                              sessionManager_; // 上游会话管理器指针 
};

} // namespace session
} // namespace http