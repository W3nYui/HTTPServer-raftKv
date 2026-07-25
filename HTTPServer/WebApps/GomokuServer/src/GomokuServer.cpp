#include "../include/handlers/EntryHandler.h"
#include "../include/handlers/LoginHandler.h"
#include "../include/handlers/RegisterHandler.h"
#include "../include/handlers/MenuHandler.h"
#include "../include/handlers/AiGameStartHandler.h"
#include "../include/handlers/LogoutHandler.h"
#include "../include/handlers/AiGameMoveHandler.h"
#include "../include/handlers/GameBackendHandler.h"
#include "../include/handlers/GameWsHandler.h"
#include "../include/GomokuServer.h"
#include "../include/GameRoom.h"
#include "../../../HttpServer/include/http/HttpRequest.h"
#include "../../../HttpServer/include/http/HttpResponse.h"
#include "../../../HttpServer/include/http/HttpServer.h"
#include "../../../HttpServer/include/ssl/SslConfig.h"
#include "../../../HttpServer/include/ssl/SslTypes.h"

#include <stdexcept>


using namespace http;
// 应用层服务器 初始化 调度底层网络层
GomokuServer::GomokuServer(int port,
                           const std::string &name,
                           bool useSSL,
                           std::unique_ptr<GameStateStore> gameStateStore,
                           muduo::net::TcpServer::Option option)
    : httpServer_(port, name, useSSL, option),
      maxOnline_(0),
      gameStateStore_(std::move(gameStateStore)) // 这里httpServer_是muduo的HttpServer实例，用于处理HTTP请求 但是传参不匹配
{
    if (!gameStateStore_) throw std::invalid_argument("PVP game state store is required");
    pvpGameService_ = std::make_unique<PvpGameService>(*gameStateStore_);
    recoverActiveGameRooms();
    initialize(); // 初始化会话管理、中间件管理、路由
    if (useSSL)
    {
        ssl::SslConfig sslConfig;
        // 使用绝对路径确保在任何工作目录下都能找到证书文件
        sslConfig.setCertificateFile("/home/w3nyui/Learning/HTTPServer/certs/server.crt");
        sslConfig.setPrivateKeyFile("/home/w3nyui/Learning/HTTPServer/certs/server.key");
        sslConfig.setCertificateChainFile("/home/w3nyui/Learning/HTTPServer/certs/ca.crt");
        sslConfig.setProtocolVersion(ssl::SSLVersion::TLS_1_2);
        sslConfig.setCipherList("HIGH:!aNULL:!MD5");
        sslConfig.setVerifyClient(false);

        httpServer_.setSslConfig(sslConfig); // 给httpserver设置公共的ssl配置 用于所有连接的ssl连接
    }
}

void GomokuServer::recoverActiveGameRooms()
{
    const auto snapshots = pvpGameService_->recoverActiveRooms();
    std::lock_guard<std::mutex> lock(mutexForGameRooms_);
    for (const auto& snapshot : snapshots)
    {
        try
        {
            auto room = GameRoom::fromSnapshot(snapshot);
            gameRooms_[room->roomId()] = std::move(room);
        }
        catch (const std::exception& error)
        {
            LOG_WARN << "Ignoring invalid recovered PVP room: " << error.what();
        }
    }
}

void GomokuServer::setThreadNum(int numThreads)
{
    httpServer_.setThreadNum(numThreads);
}

void GomokuServer::start()
{
    httpServer_.start();
}

void GomokuServer::initialize()
{
    // 初始化数据库连接池
    // mysql是使用tcp协议进行连接的 因为指定了mysql的监听端口为3306 所以这里也指定了3306 同时设定连接池大小为10(只有10个线程会同时连接数据库)
    http::MysqlUtil::init("tcp://127.0.0.1:3306", "root", "root", "Gomoku", 10);
    // 初始化会话
    initializeSession(); // session 用来存储用户登录状态 因为http是无状态的 所以需要使用session来存储控制用户登录状态、游戏状态、访问权限等
    // 初始化中间件
    initializeMiddleware();  // 中间件是在http处理流程中插入的可插拔功能模块 用于在到达路由之前或返回客户端之前执行一些处理
    // 初始化路由
    initializeRouter();
}

void GomokuServer::initializeSession()
{
    // session -> sessionStorage -> sessionManager
    // 创建会话存储类对象
    auto sessionStorage = std::make_unique<http::session::MemorySessionStorage>();
    // 创建会话管理器 管理会话存储类 即对外接口
    auto sessionManager = std::make_unique<http::session::SessionManager>(std::move(sessionStorage));
    // 设置会话管理器
    setSessionManager(std::move(sessionManager));
}

void GomokuServer::initializeMiddleware()
{
    // 创建中间件
    auto corsMiddleware = std::make_shared<http::middleware::CorsMiddleware>();
    // 限流中间件 这里用默认配置
    auto rateLimiterMiddleware = std::make_shared<http::middleware::RateLimiterMiddleware>();
    // 日志中间件
    auto loggingMiddleware = std::make_shared<http::middleware::RequestLoggingMiddleware>();
    // 添加中间件 先添加日志中间件 再添加限流中间件 最后添加cors中间件 因为cors中间件只用于检查请求段
    httpServer_.addMiddleware(loggingMiddleware);
    httpServer_.addMiddleware(rateLimiterMiddleware);
    httpServer_.addMiddleware(corsMiddleware);
    
}

/**
 * @brief 初始化路由
 * 注册url回调处理器 分别使用了注册对象和注册回调式
 * @param response HTTP响应
 */
void GomokuServer::initializeRouter()
{
    // 注册url回调处理器
    // get用于获取数据 post用于提交数据
    // 登录注册入口页面
    httpServer_.Get("/", std::make_shared<EntryHandler>(this));
    httpServer_.Get("/entry", std::make_shared<EntryHandler>(this));
    // 登录
    httpServer_.Post("/login", std::make_shared<LoginHandler>(this));
    // 注册
    httpServer_.Post("/register", std::make_shared<RegisterHandler>(this));
    // 登出
    httpServer_.Post("/user/logout", std::make_shared<LogoutHandler>(this));
    // 菜单页面
    httpServer_.Get("/menu", std::make_shared<MenuHandler>(this));
    // 开始对战ai -> 返回对战界面
    httpServer_.Get("/aiBot/start", std::make_shared<AiGameStartHandler>(this));
    // 下棋
    httpServer_.Post("/aiBot/move", std::make_shared<AiGameMoveHandler>(this));
    // 重新开始对战ai
    httpServer_.Get("/aiBot/restart", 
    [this](const http::HttpRequest& req, http::HttpResponse* resp) { // 采用注册回调式处理 因为逻辑简单
            restartChessGameVsAi(req, resp);
    });

    // 后台界面 会执行一次后台数据获取
    httpServer_.Get("/backend", std::make_shared<GameBackendHandler>(this));
    // 后台数据获取
    httpServer_.Get("/backend_data", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        getBackendData(req, resp);
    });
    // 直接返回测试信息 用于测试限流中间件
    httpServer_.Get("/api/test", [](const http::HttpRequest& req, http::HttpResponse* resp) {
        resp->setStatusCode(http::HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        std::string body = R"({"status": "ok", "message": "Test endpoint"})";
        resp->setBody(body);
    });

    // WebSocket 升级端点（处理 PVP 匹配、落子、聊天等所有实时通信）
    wsHandler_ = std::make_shared<GameWsHandler>(this);
    httpServer_.Get("/ws", wsHandler_);

    // PVP 对战页面（返回 ChessGameVsPlayer.html）
    httpServer_.Get("/pvp", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        // 验证登录状态
        auto session = getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") != "true")
        {
            json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);
            packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                        "Unauthorized", true, "application/json", errorBody.size(),
                        errorBody, resp);
            return;
        }

        std::string userIdStr = session->getValue("userId");
        std::string username  = session->getValue("username");

        // 返回 PVP 对战页面
        FileUtil fileUtil("../WebApps/GomokuServer/resource/ChessGameVsPlayer.html");
        if (!fileUtil.isValid())
        {
            LOG_WARN << "ChessGameVsPlayer.html not found";
            resp->setStatusCode(http::HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
            return;
        }
        std::vector<char> content(fileUtil.size());
        fileUtil.readFile(content);
        std::string contentStr(content.begin(), content.end());

        // 注入 userId 和 username 供前端使用
        size_t headEnd = contentStr.find("</head>");
        if (headEnd != std::string::npos)
        {
            std::string script = "<script>"
                "var INJECTED_USERID = '" + userIdStr + "';"
                "var INJECTED_USERNAME = '" + username + "';"
                "</script>";
            contentStr.insert(headEnd, script);
        }

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setContentType("text/html");
        resp->setBody(contentStr);
        resp->setContentLength(contentStr.size());
        resp->setCloseConnection(false);
    });
}

// ========== PVP 房间管理 ==========
int GomokuServer::createGameRoom(int player1, int player2)
{
    const auto created = pvpGameService_->createRoom(player1, player2);
    if (created.status != PvpGameStatus::kOk) return 0;

    auto room = GameRoom::fromSnapshot(created.snapshot);
    const int roomId = room->roomId();

    std::lock_guard<std::mutex> lock(mutexForGameRooms_);
    gameRooms_[roomId] = room;

    LOG_INFO << "GameRoom created: roomId=" << roomId
             << " player1=" << player1 << " player2=" << player2;
    return roomId;
}

std::shared_ptr<GameRoom> GomokuServer::getGameRoom(int roomId)
{
    {
        std::lock_guard<std::mutex> lock(mutexForGameRooms_);
        auto it = gameRooms_.find(roomId);
        if (it != gameRooms_.end()) return it->second;
    }

    const auto loaded = pvpGameService_->load(roomId);
    if (loaded.status != PvpGameStatus::kOk) return nullptr;

    auto room = GameRoom::fromSnapshot(loaded.snapshot);
    std::lock_guard<std::mutex> lock(mutexForGameRooms_);
    gameRooms_[roomId] = room;
    return room;
}

PvpGameResult GomokuServer::moveGameRoom(int roomId, int playerId, int x, int y)
{
    const auto result = pvpGameService_->move(roomId, playerId, x, y);
    if (result.status != PvpGameStatus::kOk) return result;

    auto room = GameRoom::fromSnapshot(result.snapshot);
    std::lock_guard<std::mutex> lock(mutexForGameRooms_);
    gameRooms_[roomId] = room;
    return result;
}

PvpGameResult GomokuServer::loadGameRoom(int roomId)
{
    const auto result = pvpGameService_->load(roomId);
    if (result.status != PvpGameStatus::kOk) return result;

    auto room = GameRoom::fromSnapshot(result.snapshot);
    std::lock_guard<std::mutex> lock(mutexForGameRooms_);
    gameRooms_[roomId] = std::move(room);
    return result;
}

PvpGameResult GomokuServer::finishGameRoom(int roomId, int winnerId)
{
    const auto result = pvpGameService_->finish(roomId, winnerId);
    if (result.status != PvpGameStatus::kOk) return result;

    auto room = GameRoom::fromSnapshot(result.snapshot);
    std::lock_guard<std::mutex> lock(mutexForGameRooms_);
    gameRooms_[roomId] = room;
    return result;
}

int GomokuServer::getRoomByUserId(int userId) const
{
    std::lock_guard<std::mutex> lock(mutexForGameRooms_);
    for (const auto& [roomId, room] : gameRooms_)
    {
        if (room->player1() == userId || room->player2() == userId)
        {
            return roomId;
        }
    }
    return 0;
}

void GomokuServer::removeGameRoom(int roomId)
{
    std::lock_guard<std::mutex> lock(mutexForGameRooms_);
    auto it = gameRooms_.find(roomId);
    if (it != gameRooms_.end())
    {
        LOG_INFO << "GameRoom removed: roomId=" << roomId;
        gameRooms_.erase(it);
    }
}

void GomokuServer::restartChessGameVsAi(const http::HttpRequest &req, http::HttpResponse *resp)
{
    // 解析请求体
    auto session = getSessionManager()->getSession(req, resp);
    if (session->getValue("isLoggedIn") != "true")
    {
        // 用户未登录，返回未授权错误
        json errorResp;
        errorResp["status"] = "error";
        errorResp["message"] = "Unauthorized";
        std::string errorBody = errorResp.dump(4);

        packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                    "Unauthorized", true, "application/json", errorBody.size(),
                    errorBody, resp);
        return;
    }

    int userId = std::stoi(session->getValue("userId"));
    {
        // 重新开始ai对战
        std::lock_guard<std::mutex> lock(mutexForAiGames_); 
        if (aiGames_.find(userId) != aiGames_.end()) // 如果用户正在游戏中 则先删除旧的游戏实例 然后创建新的游戏实例 作为新的游戏状态
            aiGames_.erase(userId);
        aiGames_[userId] = std::make_shared<AiGame>(userId);
    }

    json successResp;
    successResp["status"] = "ok";
    successResp["message"] = "restart successful";
    successResp["userId"] = userId;
    std::string successBody = successResp.dump(4);
    packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK", false, "application/json", successBody.size(), successBody, resp);
}

// 获取后台数据
void GomokuServer::getBackendData(const http::HttpRequest &req, http::HttpResponse *resp)
{
    try 
    {
        // 获取数据
        int curOnline = getCurOnline();
        LOG_INFO << "当前在线人数: " << curOnline;
        
        int maxOnline = getMaxOnline();
        LOG_INFO << "历史最高在线人数: " << maxOnline;
        
        int totalUser = getUserCount();
        LOG_INFO << "已注册用户总数: " << totalUser;

        // 构造 JSON 响应
        nlohmann::json respBody;
        respBody = {
            {"curOnline", curOnline},
            {"maxOnline", maxOnline},
            {"totalUser", totalUser}
        };

        // 转换为字符串
        std::string responseStr = respBody.dump(4);
        
        // 设置响应
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setContentType("application/json");
        resp->setBody(responseStr);
        resp->setContentLength(responseStr.size());
        resp->setCloseConnection(false);

        LOG_INFO << "Backend data response prepared successfully";
    }
    catch (const std::exception& e) 
    {
        LOG_ERROR << "Error in getBackendData: " << e.what();
        
        // 错误响应
        nlohmann::json errorBody = {
            {"error", "Internal Server Error"},
            {"message", e.what()}
        };
        
        std::string errorStr = errorBody.dump();
        resp->setStatusCode(http::HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setContentType("application/json");
        resp->setBody(errorStr);
        resp->setContentLength(errorStr.size());
        resp->setCloseConnection(true);
    }
}

void GomokuServer::packageResp(const std::string &version,
                             http::HttpResponse::HttpStatusCode statusCode,
                             const std::string &statusMsg,
                             bool close,
                             const std::string &contentType,
                             int contentLen,
                             const std::string &body,
                             http::HttpResponse *resp)
{
    if (resp == nullptr) 
    {
        LOG_ERROR << "Response pointer is null";
        return;
    }

    try 
    {
        resp->setVersion(version);
        resp->setStatusCode(statusCode);
        resp->setStatusMessage(statusMsg);
        resp->setCloseConnection(close);
        resp->setContentType(contentType);
        resp->setContentLength(contentLen);
        resp->setBody(body);
        
        LOG_INFO << "Response packaged successfully";
    }
    catch (const std::exception& e) 
    {
        LOG_ERROR << "Error in packageResp: " << e.what();
        // 设置一个基本的错误响应
        resp->setStatusCode(http::HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setCloseConnection(true);
    }
}

