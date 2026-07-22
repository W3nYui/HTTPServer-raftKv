#include "../../include/handlers/AiGameStartHandler.h"

void AiGameStartHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    auto session = server_->getSessionManager()->getSession(req, resp);
    if (session->getValue("isLoggedIn") != "true")
    {
        // 用户未登录，返回未授权错误
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

    // 看来需要menu页面post发送userId
    {
        std::lock_guard<std::mutex> lock(server_->mutexForAiGames_);
        if (server_->aiGames_.find(userId) != server_->aiGames_.end())
            server_->aiGames_.erase(userId);
        server_->aiGames_[userId] = std::make_shared<AiGame>(userId); // 获得当前用户id的ai游戏实例
    }

    // 创建一个ai机器人，它就while不断地执行下棋逻辑
    std::string reqFile("../WebApps/GomokuServer/resource/ChessGameVsAi.html");
    FileUtil fileOperater(reqFile);
    if (!fileOperater.isValid())
    {
        LOG_WARN << reqFile << "not exist.";
        fileOperater.resetDefaultFile(); // 重置为默认文件 但可能不必要
    }

    std::vector<char> buffer(fileOperater.size());
    fileOperater.readFile(buffer);
    std::string htmlContent(buffer.data(), buffer.size());

    resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
    resp->setCloseConnection(false);
    resp->setContentType("text/html");
    resp->setContentLength(htmlContent.size());
    resp->setBody(htmlContent); // 返回对战界面
}
