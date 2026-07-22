#include "../../include/handlers/RegisterHandler.h"
#include <cppconn/resultset.h>

void RegisterHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    // 将请求体内的json字符串解析为c++中的json对象
    json parsed = json::parse(req.getBody()); // 前端通过fetch发送json格式的请求体 json的内容是提前定义的格式
    std::string username = parsed["username"];
    std::string password = parsed["password"];

    // 判断用户是否已经存在，如果存在则注册失败
    int userId = insertUser(username, password);
    if (userId != -1 && userId != -2)
    {
        // 插入成功
        // 封装成功响应
        json successResp;   
        successResp["status"] = "success";
        successResp["message"] = "Register successful";
        successResp["userId"] = userId;
        std::string successBody = successResp.dump(4); // 格式化json字符串 4个空格缩进

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
    }
    else if (userId == -1)
    {
        // 注册失败 用户名已存在
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = "username already exists";
        std::string failureBody = failureResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k409Conflict, "Conflict");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    } else {
        // 插入失败
        json insertFailureResp;
        insertFailureResp["status"] = "error";
        insertFailureResp["message"] = "insert failed";
        std::string insertFailureBody = insertFailureResp.dump(4);
        // 返回500 服务器内部错误
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(insertFailureBody.size());
        resp->setBody(insertFailureBody);
    }
}

int RegisterHandler::insertUser(const std::string &username, const std::string &password)
{
    // 判断用户是否存在，如果存在则返回-1，否则返回用户id
    if (!isUserExist(username))
    {
        // 用户不存在，插入用户
        // std::string sql = "INSERT INTO users (username, password) VALUES ('" + username + "', '" + password + "')";
        // mysqlUtil_.executeUpdate(sql);
        // std::string sql2 = "SELECT id FROM users WHERE username = '" + username + "'";
        // sql::ResultSet* res = mysqlUtil_.executeQuery(sql2);
        
        // 使用预处理语句 避免SQL注入攻击
        std::string sql = "INSERT INTO users (username, password) VALUES (?, ?)";
        mysqlUtil_.executeUpdate(sql, username, password);

        // sql::ResultSet* res = mysqlUtil_.executeQuery(sql2, username);

        // if (res->next()) // 如果查询结果集有数据
        // {
        //     return res->getInt("id");
        // } else {
        //     return -2; // 插入失败 返回-2
        // }

        std::string sql2 = "SELECT id FROM users WHERE username = ?";
        sql::ResultSet* res;
        try {
            res = mysqlUtil_.executeQuery(sql2, username);
        } catch (const sql::SQLException& e) {
            return -2; // 插入失败 返回-2
        } 
        if (res->next())
        {
            return res->getInt("id"); // 返回用户id 插入成功
        }
    }
    return -1; // 用户已经存在 返回-1 或者查询失败 返回-1
}

bool RegisterHandler::isUserExist(const std::string &username)
{
    // std::string sql = "SELECT id FROM users WHERE username = '" + username + "'";
    // sql::ResultSet* res = mysqlUtil_.executeQuery(sql);
    std::string sql = "SELECT id FROM users WHERE username = ?";
    // sql::ResultSet* res = mysqlUtil_.executeQuery(sql, username);
    // if (res->next())
    // {
    //     return true;
    // }
    // return false;
    sql::ResultSet* res;
    try {
        res = mysqlUtil_.executeQuery(sql, username);
    } catch (const sql::SQLException& e) {
        return false; // 查询失败 返回false
    } 
    if (res->next())
    {
        return true; // 用户存在 返回true
    }
    return false; // 用户不存在 返回false
}