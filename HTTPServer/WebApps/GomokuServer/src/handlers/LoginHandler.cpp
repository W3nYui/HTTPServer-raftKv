#include "../../include/handlers/LoginHandler.h"

void LoginHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    // 处理登录逻辑
    // 验证 contentType
    auto contentType = req.getHeader("Content-Type");
    // 如果请求头中没有 Content-Type 或者 Content-Type 不是 application/json 或者请求体为空，都返回 400 Bad Request
    if (contentType.empty() || contentType != "application/json" || req.getBody().empty())
    {
        LOG_INFO << "content" << req.getBody();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true); // 关闭连接 因为该请求失败，客户端需要重新发送请求
        resp->setContentType("application/json");
        resp->setContentLength(0);
        resp->setBody("");
        return;
    }

    // JSON 解析使用 try catch 捕获异常
    try
    {
        // 解析 JSON体
        json parsed = json::parse(req.getBody());
        std::string username = parsed["username"];
        std::string password = parsed["password"];
        // 验证用户是否存在
        int userId = queryUserId(username, password);
        if (userId != -1)
        {
            // 如果用户不在在线用户列表中 将用户添加到在线用户列表中
            if (server_->onlineUsers_.find(userId) == server_->onlineUsers_.end() || server_->onlineUsers_[userId] == false)
            {
                // 更新session 或创建新会话
                auto session = server_->getSessionManager()->getSession(req, resp);
                // 会话都不是同一个会话，因为会话判断是不是同一个会话是通过请求报文中的cookie来判断的
                // 所以不同页面的访问是不可能是相同的会话的，只有该页面前面访问过服务端，才会有会话记录
                // 那么判断用户是否在其他地方登录中不能通过会话来判断
                
                // 在会话中存储用户信息
                session->setValue("userId", std::to_string(userId));
                session->setValue("username", username);
                session->setValue("isLoggedIn", "true");
                // 更新在线用户列表
                {
                    std::lock_guard<std::mutex> lock(server_->mutexForOnlineUsers_);
                    server_->onlineUsers_[userId] = true;
                }
                
                // 更新历史最高在线人数
                server_->updateMaxOnline(server_->onlineUsers_.size());
                // 用户存在登录成功
                // 封装json 数据。
                json successResp;
                successResp["success"] = true;
                successResp["userId"] = userId;
                successResp["username"] = username;
                std::string successBody = successResp.dump(4); // 格式化 JSON 字符串，缩进 4 个空格

                resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
                resp->setCloseConnection(false);
                resp->setContentType("application/json"); // 设置响应头中的 Content-Type 为 application/json
                resp->setContentLength(successBody.size());
                resp->setBody(successBody);
                return;
            }
            else // 用户已在其他地方登录
            {
                // FIXME: 当前该用户正在其他地方登录中，将原有登录用户强制下线更好 可以通过判断会话存在/session内增加字段来特判被踢下机的用户
                // 不允许重复登录，
                json failureResp;
                failureResp["success"] = false;
                failureResp["error"] = "账号已在其他地方登录";
                std::string failureBody = failureResp.dump(4);

                resp->setStatusLine(req.getVersion(), http::HttpResponse::k403Forbidden, "Forbidden");
                resp->setCloseConnection(true);
                resp->setContentType("application/json");
                resp->setContentLength(failureBody.size());
                resp->setBody(failureBody);
                return;
            }
        }
        else // 账号密码错误，请重新登录
        {
            // 封装json数据
            json failureResp;
            failureResp["status"] = "error";
            failureResp["message"] = "Invalid username or password";
            std::string failureBody = failureResp.dump(4);

            resp->setStatusLine(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(failureBody.size());
            resp->setBody(failureBody);
            return;
        }
    }
    catch (const std::exception &e)
    {
        // 捕获异常，返回错误信息
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
        return;
    }
}

int LoginHandler::queryUserId(const std::string &username, const std::string &password)
{
    // 前端用户传来账号密码，查找数据库是否有该账号密码
    // 使用预处理语句, 防止sql注入
    std::string sql = "SELECT id FROM users WHERE username = ? AND password = ?"; // 使用?占位符
    // std::vector<std::string> params = {username, password};
    sql::ResultSet* res = mysqlUtil_.executeQuery(sql, username, password);
    if (res->next())
    {
        int id = res->getInt("id");
        return id;
    }
    // 如果查询结果为空，则返回-1
    return -1;
}

