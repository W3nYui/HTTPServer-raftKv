#include "../../include/handlers/EntryHandler.h"

void EntryHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::string reqFile;
    reqFile.append("../WebApps/GomokuServer/resource/entry.html");
    FileUtil fileOperater(reqFile);
    if (!fileOperater.isValid())
    {
        LOG_WARN << reqFile << " not exist";
        fileOperater.resetDefaultFile();
    }

    if (!fileOperater.isValid())
    {
        std::string body = "<html><body><h1>404 Not Found</h1></body></html>";
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k404NotFound, "Not Found");
        resp->setCloseConnection(true);
        resp->setContentType("text/html");
        resp->setContentLength(body.size());
        resp->setBody(body);
        return;
    }

    std::vector<char> buffer(fileOperater.size());
    fileOperater.readFile(buffer);
    std::string bufStr = std::string(buffer.data(), buffer.size());
    
    resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
    resp->setCloseConnection(false);
    resp->setContentType("text/html");
    resp->setContentLength(bufStr.size());
    resp->setBody(bufStr);
}
