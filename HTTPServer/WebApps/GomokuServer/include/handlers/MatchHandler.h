#pragma once

#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../GomokuServer.h"

class MatchHandler : public http::router::RouterHandler
{
public:
    explicit MatchHandler(GomokuServer* server, bool isJoin) : server_(server), isJoin_(isJoin) {}

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    GomokuServer* server_;
    bool isJoin_;
};
