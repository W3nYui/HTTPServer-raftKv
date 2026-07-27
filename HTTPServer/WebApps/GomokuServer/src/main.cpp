#include <iostream>
#include <string>
#include <thread>

#include <muduo/base/Logging.h>
#include <muduo/net/TcpServer.h>

#include "../include/GomokuServer.h"
#include "../include/RaftGameStateStore.h"
#include "../../../HttpServer/include/http/HttpServer.h"

namespace
{
void startHttpRedirectServer(int httpPort, int httpsPort)
{
  http::HttpServer redirectServer(httpPort, "Gomoku HTTP redirect");
  redirectServer.setHttpCallback([httpsPort](const http::HttpRequest& req, http::HttpResponse* resp) {
    resp->setStatusLine(req.getVersion(), http::HttpResponse::k308PermanentRedirect, "Permanent Redirect");
    resp->setCloseConnection(true);
    resp->setContentLength(0);
    std::string location = "https://127.0.0.1:" + std::to_string(httpsPort) + req.path();
    if (!req.queryString().empty())
    {
      location += "?" + req.queryString();
    }
    resp->addHeader("Location", location);
    resp->addHeader("Cache-Control", "no-store");
  });
  redirectServer.start();
}
} // namespace

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();

  int httpPort = 8080;
  int httpsPort = 8443;
  std::string raftConfig;
  std::string certificateFile;
  std::string privateKeyFile;

  int opt;
  const char* options = "p:P:r:c:k:";
  while ((opt = getopt(argc, argv, options)) != -1)
  {
    switch (opt)
    {
      case 'p':
        httpPort = atoi(optarg);
        break;
      case 'P':
        httpsPort = atoi(optarg);
        break;
      case 'r':
        raftConfig = optarg;
        break;
      case 'c':
        certificateFile = optarg;
        break;
      case 'k':
        privateKeyFile = optarg;
        break;
      default:
        break;
    }
  }

  if (raftConfig.empty())
  {
    LOG_ERROR << "Raft config is required: start with -r <raft-nodes.conf>";
    return 1;
  }
  if (certificateFile.empty() || privateKeyFile.empty())
  {
    LOG_ERROR << "TLS certificate and private key are required: start with -c <server.crt> -k <server.key>";
    return 1;
  }
  if (httpPort <= 0 || httpsPort <= 0 || httpPort == httpsPort)
  {
    LOG_ERROR << "HTTP and HTTPS ports must be distinct positive values";
    return 1;
  }

  muduo::Logger::setLogLevel(muduo::Logger::WARN);
  const auto option = muduo::net::TcpServer::kNoReusePort;
  auto raftClient = std::make_shared<ClerkRaftGameStateClient>(raftConfig);
  auto gameStateStore = std::make_unique<RaftGameStateStore>(std::move(raftClient));

  std::thread([httpPort, httpsPort] {
    try
    {
      startHttpRedirectServer(httpPort, httpsPort);
    }
    catch (const std::exception& error)
    {
      LOG_ERROR << "HTTP redirect server stopped: " << error.what();
    }
  }).detach();

  GomokuServer server(httpsPort, "Gomoku HTTPS server", true, std::move(gameStateStore),
                      certificateFile, privateKeyFile, option);
  // TLS state is connection-affine; one I/O loop keeps local PVP sockets serialized.
  server.setThreadNum(1);
  server.start();
}
