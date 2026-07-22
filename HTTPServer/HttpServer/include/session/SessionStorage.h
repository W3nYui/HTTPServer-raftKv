#pragma once
#include "Session.h"
#include <memory>

namespace http
{
namespace session
{
/**
 * @brief 会话存储类的抽象类 用于定义会话存储的接口
 * 定义了写入接口、加载接口、删除接口
 * */
class SessionStorage
{
public:
    virtual ~SessionStorage() = default;
    virtual void save(std::shared_ptr<Session> session) = 0;
    virtual std::shared_ptr<Session> load(const std::string& sessionId) = 0;
    virtual void remove(const std::string& sessionId) = 0;
};

// 基于内存的会话存储实现
class MemorySessionStorage : public SessionStorage
{
public:
    void save(std::shared_ptr<Session> session) override; // 存储一个会话对象
    std::shared_ptr<Session> load(const std::string& sessionId) override; // 加载一个会话对象
    void remove(const std::string& sessionId) override; // 删除一个会话对象
private:
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
};

} // namespace session
} // namespace http