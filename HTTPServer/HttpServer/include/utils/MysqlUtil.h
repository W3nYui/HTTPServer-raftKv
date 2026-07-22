 #pragma once
 #include "db/DbConnectionPool.h"
 
#include <string>

namespace http
{
// 定义的mysql工具类 用于封装mysql池的操作逻辑 利用单例模式 初始化连接池 并提供查询和更新操作
class MysqlUtil
{
public:
    static void init(const std::string& host, const std::string& user,
                    const std::string& password, const std::string& database,
                    size_t poolSize = 10)
    {
        http::db::DbConnectionPool::getInstance().init( // 运用单例模式 初始化连接池
            host, user, password, database, poolSize);
    }

    template<typename... Args> // 利用模板 来处理mysql查询语句内的各种占位符
    // 返回查询结果集的指针 用于处理查询操作
    sql::ResultSet* executeQuery(const std::string& sql, Args&&... args)
    {
        // getConnection 用于从连接池获取一个连接实例 并返回一个指针指向该连接实例
        auto conn = http::db::DbConnectionPool::getInstance().getConnection();
        return conn->executeQuery(sql, std::forward<Args>(args)...); // 完美转发参数 并返回查询结果集
    }

    template<typename... Args>
    /**
     * @brief 执行更新操作
     * 
     * @param sql 更新语句
     * @param args 更新语句中的占位符参数
     * @return int 更新的行数
     */
    int executeUpdate(const std::string& sql, Args&&... args)
    {
        auto conn = http::db::DbConnectionPool::getInstance().getConnection();
        return conn->executeUpdate(sql, std::forward<Args>(args)...);
    }
};

} // namespace http
