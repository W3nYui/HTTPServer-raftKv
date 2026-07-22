#pragma once
#include <stdexcept>
#include <string>

namespace http {
namespace db {
// 定义的数据库异常类 继承了runtime_error异常类 用于表示数据库操作中的异常情况
class DbException : public std::runtime_error 
{
public:
    explicit DbException(const std::string& message) 
        : std::runtime_error(message) {}
    
    explicit DbException(const char* message) 
        : std::runtime_error(message) {}
};

} // namespace db
} // namespace http