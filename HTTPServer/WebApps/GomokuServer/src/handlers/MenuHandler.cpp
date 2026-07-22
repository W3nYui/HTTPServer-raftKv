#include "../../include/handlers/MenuHandler.h"

/**
 * @brief 转义字符串用于嵌入 JavaScript 字符串字面量
 * 替换 \ ' " 和换行符，防止破坏 JS 语法
 */
static std::string escapeForJsString(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s)
    {
        switch (c)
        {
            case '\\': out += "\\\\"; break;
            case '\'': out += "\\'";  break;
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

void MenuHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    try
    {
        // 检查用户是否已登录
        auto session = server_->getSessionManager()->getSession(req, resp);
        LOG_INFO << "session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
        if (session->getValue("isLoggedIn") != "true")
        {
            json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);

            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                                "Unauthorized", true, "application/json", errorBody.size(),
                                 errorBody, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");

        std::string reqFile("../WebApps/GomokuServer/resource/menu.html");
        FileUtil fileOperater(reqFile);
        if (!fileOperater.isValid())
        {
            LOG_WARN << reqFile << "not exist.";
            fileOperater.resetDefaultFile();
        }

        std::vector<char> buffer(fileOperater.size());
        fileOperater.readFile(buffer);
        std::string htmlContent(buffer.data(), buffer.size());

        // 注入 userId 和 username（转义防止特殊字符破坏JS语法）
        size_t headEnd = htmlContent.find("</head>");
        if (headEnd != std::string::npos)
        {
            std::string script = "<script>"
                "var INJECTED_USERID = '" + std::to_string(userId) + "';"
                "var INJECTED_USERNAME = '" + escapeForJsString(username) + "';"
                "</script>";
            htmlContent.insert(headEnd, script);
        }

        server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK"
                    , false, "text/html", htmlContent.size(), htmlContent, resp);
    }
    catch (const std::exception &e)
    {
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}
