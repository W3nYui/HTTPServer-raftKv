#include "../../include/handlers/GameWsHandler.h"

#include <muduo/base/Logging.h>

#include "../../../../HttpServer/include/http/HttpRequest.h"
#include "../../../../HttpServer/include/http/HttpResponse.h"
#include "../../../../HttpServer/include/utils/JsonUtil.h"
#include "../../../../HttpServer/include/websocket/WebSocketFrame.h"

using namespace http;
using json = nlohmann::json;

GameWsHandler::GameWsHandler(GomokuServer* server)
    : server_(server)
{
}

// ========== HTTP 升级握手 ==========
void GameWsHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    // 1. 验证用户登录状态
    auto session = server_->getSessionManager()->getSession(req, resp);
    if (session->getValue("isLoggedIn") != "true")
    {
        resp->setStatusCode(http::HttpResponse::k401Unauthorized);
        resp->setStatusMessage("Unauthorized");
        resp->setCloseConnection(true);
        return;
    }

    int userId = std::stoi(session->getValue("userId"));

    // 2. 获取 TCP 连接指针（在 HttpServer::onRequest 中设置）
    const auto* connPtr = static_cast<const muduo::net::TcpConnectionPtr*>(req.getConnectionPtr());
    if (!connPtr)
    {
        resp->setStatusCode(http::HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setCloseConnection(true);
        return;
    }

    // 3. 执行 WebSocket 升级
    auto& wsServer = server_->getHttpServer().getWsServer();
    auto self = std::static_pointer_cast<http::websocket::WebSocketHandler>(
        shared_from_this());
    bool success = wsServer.handleUpgrade(req, resp, self, *connPtr);

    if (success)
    {
        // 4. 记录 userId 关联
        setUserId(*connPtr, userId);

        // 5. 加入大厅聊天
        server_->getChatManager().addToLobby(userId, *connPtr);

        LOG_WARN << "WebSocket upgrade success, userId=" << userId;
    }
}

// ========== WebSocket 连接建立 ==========
void GameWsHandler::onOpen(const TcpConnectionPtr& conn)
{
    LOG_INFO << "WebSocket connection opened: " << conn->name();
}

// ========== WebSocket 消息处理 ==========
void GameWsHandler::onMessage(const TcpConnectionPtr& conn, const std::string& message)
{
    try
    {
        json msg = json::parse(message);
        std::string type = msg.value("type", "");

        int userId = getUserId(conn);

        if (type == "match_request")
        {
            handleMatchRequest(conn, userId, msg);
        }
        else if (type == "match_cancel")
        {
            handleMatchCancel(userId);
        }
        else if (type == "move")
        {
            handleMove(conn, userId, msg);
        }
        else if (type == "state_request")
        {
            handleStateRequest(conn, userId);
        }
        else if (type == "chat_lobby")
        {
            handleChatLobby(userId, msg);
        }
        else if (type == "chat_room")
        {
            handleChatRoom(conn, userId, msg);
        }
        else if (type == "leave_game")
        {
            handleLeaveGame(userId);
        }
        else
        {
            LOG_WARN << "Unknown WebSocket message type: " << type;
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "WebSocket message parse error: " << e.what() << ", raw: " << message;
    }
}

// ========== WebSocket 连接关闭 ==========
void GameWsHandler::onClose(const TcpConnectionPtr& conn)
{
    int userId = getUserId(conn);
    if (userId > 0)
    {
        // 从匹配池移除
        server_->getMatchmakingPool().leaveQueue(userId);

        // 从大厅移除
        server_->getChatManager().removeFromLobby(userId);

        // 处理游戏中的断开
        int roomId = server_->getRoomByUserId(userId);
        if (roomId > 0)
        {
            auto room = server_->getGameRoom(roomId);
            if (room)
            {
                int opponentId = room->getOpponent(userId);

                auto oppConn = getConnectionByUserId(opponentId);
                // 标记对手获胜
                if (server_->finishGameRoom(roomId, opponentId).status != PvpGameStatus::kOk)
                {
                    LOG_WARN << "Failed to finish room after disconnect: " << roomId;
                }
                else
                {
                    // 通知对手
                    if (oppConn && oppConn->connected())
                    {
                        json notify;
                        notify["type"] = "opponent_left";
                        notify["message"] = "对手已断开连接";
                        auto& wsServer = server_->getHttpServer().getWsServer();
                        wsServer.sendMessage(oppConn, notify.dump());
                    }

                    // 将对手移回大厅
                    if (opponentId > 0)
                    {
                        server_->getChatManager().addToLobby(opponentId, oppConn);
                    }

                    // 清理房间
                    server_->removeGameRoom(roomId);
                }
            }
        }

        LOG_INFO << "User " << userId << " disconnected from WebSocket";
    }

    // 清理映射
    removeUserId(conn);
}

// ========== 匹配请求 ==========
void GameWsHandler::handleMatchRequest(const TcpConnectionPtr& conn, int userId,
                                        const nlohmann::json& msg)
{
    auto& pool = server_->getMatchmakingPool();
    int result = pool.tryMatch(userId);

    if (result >= 0)
    {
        // 匹配成功！对手 ID = result
        int opponent = result;
        auto oppConn = getConnectionByUserId(opponent);

        // 创建对局房间
        auto& wsServer = server_->getHttpServer().getWsServer();
        int roomId = server_->createGameRoom(userId, opponent);
        if (roomId <= 0)
        {
            json unavailable;
            unavailable["type"] = "raft_unavailable";
            unavailable["roomId"] = 0;
            wsServer.sendMessage(conn, unavailable.dump());
            return;
        }

        // 从大厅移除双方
        server_->getChatManager().removeFromLobby(userId);
        server_->getChatManager().removeFromLobby(opponent);

        // 通知 player1（黑棋，先手）
        json matchFound1;
        matchFound1["type"] = "match_found";
        matchFound1["roomId"] = roomId;
        matchFound1["color"] = "black";
        matchFound1["opponent"] = opponent;
        matchFound1["yourTurn"] = true;
        wsServer.sendMessage(conn, matchFound1.dump());

        // 通知 player2（白棋，后手）
        if (oppConn && oppConn->connected())
        {
            json matchFound2;
            matchFound2["type"] = "match_found";
            matchFound2["roomId"] = roomId;
            matchFound2["color"] = "white";
            matchFound2["opponent"] = userId;
            matchFound2["yourTurn"] = false;
            wsServer.sendMessage(oppConn, matchFound2.dump());
        }

        LOG_INFO << "Match found: userId=" << userId << " vs opponent=" << opponent
                 << " roomId=" << roomId;
    }
    else if (result == -1)
    {
        // 等待匹配中
        json waiting;
        waiting["type"] = "match_waiting";
        waiting["message"] = "正在寻找对手...";
        auto& wsServer = server_->getHttpServer().getWsServer();
        wsServer.sendMessage(conn, waiting.dump());
    }
    else if (result == -2)
    {
        // 已在队列中
        json already;
        already["type"] = "match_waiting";
        already["message"] = "已在匹配队列中，请耐心等待";
        auto& wsServer = server_->getHttpServer().getWsServer();
        wsServer.sendMessage(conn, already.dump());
    }
}

// ========== 取消匹配 ==========
void GameWsHandler::handleMatchCancel(int userId)
{
    auto& pool = server_->getMatchmakingPool();
    pool.leaveQueue(userId);

    auto conn = getConnectionByUserId(userId);
    if (conn && conn->connected())
    {
        json cancel;
        cancel["type"] = "match_cancelled";
        cancel["message"] = "已取消匹配";
        auto& wsServer = server_->getHttpServer().getWsServer();
        wsServer.sendMessage(conn, cancel.dump());
    }
}

// ========== 已提交状态刷新 ==========
void GameWsHandler::handleStateRequest(const TcpConnectionPtr& conn, int userId)
{
    const int roomId = server_->getRoomByUserId(userId);
    auto& wsServer = server_->getHttpServer().getWsServer();
    if (roomId <= 0)
    {
        json error;
        error["type"] = "error";
        error["message"] = "你不在任何对局中";
        wsServer.sendMessage(conn, error.dump());
        return;
    }

    const auto result = server_->loadGameRoom(roomId);
    if (result.status == PvpGameStatus::kUnavailable)
    {
        json unavailable;
        unavailable["type"] = "raft_unavailable";
        unavailable["roomId"] = roomId;
        wsServer.sendMessage(conn, unavailable.dump());
        return;
    }
    if (result.status != PvpGameStatus::kOk)
    {
        json error;
        error["type"] = "error";
        error["message"] = "房间不存在";
        wsServer.sendMessage(conn, error.dump());
        return;
    }

    json state;
    state["type"] = "state_result";
    state["roomId"] = roomId;
    state["state"] = result.snapshot;
    wsServer.sendMessage(conn, state.dump());
}

// ========== 落子 ==========
void GameWsHandler::handleMove(const TcpConnectionPtr& conn, int userId,
                                const nlohmann::json& msg)
{
    int roomId = server_->getRoomByUserId(userId);
    if (roomId <= 0)
    {
        json error;
        error["type"] = "error";
        error["message"] = "你不在任何对局中";
        auto& wsServer = server_->getHttpServer().getWsServer();
        wsServer.sendMessage(conn, error.dump());
        return;
    }

    auto room = server_->getGameRoom(roomId);
    if (!room)
    {
        json error;
        error["type"] = "error";
        error["message"] = "房间不存在";
        auto& wsServer = server_->getHttpServer().getWsServer();
        wsServer.sendMessage(conn, error.dump());
        return;
    }

    int x = msg.value("x", -1);
    int y = msg.value("y", -1);

    const auto moveResult = server_->moveGameRoom(roomId, userId, x, y);
    auto& wsServer = server_->getHttpServer().getWsServer();

    if (moveResult.status == PvpGameStatus::kUnavailable)
    {
        json unavailable;
        unavailable["type"] = "raft_unavailable";
        unavailable["roomId"] = roomId;
        wsServer.sendMessage(conn, unavailable.dump());
        return;
    }
    if (moveResult.status == PvpGameStatus::kInvalidMove)
    {
        // 非法移动
        json error;
        error["type"] = "error";
        error["message"] = "无效的落子位置";
        wsServer.sendMessage(conn, error.dump());
        return;
    }
    else if (moveResult.status == PvpGameStatus::kNotYourTurn)
    {
        // 不是你的回合
        json error;
        error["type"] = "error";
        error["message"] = "还没轮到你落子";
        wsServer.sendMessage(conn, error.dump());
        return;
    }

    else if (moveResult.status != PvpGameStatus::kOk)
    {
        json error;
        error["type"] = "error";
        error["message"] = "game is over";
        wsServer.sendMessage(conn, error.dump());
        return;
    }

    room = server_->getGameRoom(roomId);
    if (!room) return;

    // 落子成功，通知双方
    std::string color = room->getPlayerColor(userId);
    int opponentId = room->getOpponent(userId);
    auto oppConn = getConnectionByUserId(opponentId);

    json moveResultData;
    moveResultData["type"] = "move_result";
    moveResultData["x"] = x;
    moveResultData["y"] = y;
    moveResultData["color"] = color;
    moveResultData["userId"] = userId;
    moveResultData["state"] = moveResult.snapshot;

    // 检查游戏是否结束
    if (room->isGameOver())
    {
        int winner = room->getWinner();
        std::string reason = room->getWinnerReason();

        if (winner == -1)
        {
            moveResultData["gameOver"] = true;
            moveResultData["result"] = "draw";
            moveResultData["message"] = "平局！";
        }
        else
        {
            moveResultData["gameOver"] = true;
            moveResultData["winner"] = winner;
            moveResultData["result"] = (winner == userId) ? "win" : "lose";
            moveResultData["message"] = (winner == userId) ? "你赢了！" : "你输了！";
        }
    }
    else
    {
        moveResultData["gameOver"] = false;
        moveResultData["nextTurn"] = room->currentTurn();
    }

    std::string moveResultStr = moveResultData.dump();

    // 发送给双方
    wsServer.sendMessage(conn, moveResultStr);
    if (oppConn && oppConn->connected())
    {
        wsServer.sendMessage(oppConn, moveResultStr);
    }

    // 游戏结束后的清理
    if (room->isGameOver())
    {
        // 将双方移回大厅
        server_->getChatManager().addToLobby(userId, conn);
        if (oppConn && oppConn->connected())
        {
            server_->getChatManager().addToLobby(opponentId, oppConn);
        }

        // 清理房间（延迟清理，给客户端时间处理）
        // 这里立即清理，客户端重新匹配时会创建新房间
        server_->removeGameRoom(roomId);
    }
}

// ========== 大厅聊天 ==========
void GameWsHandler::handleChatLobby(int userId, const nlohmann::json& msg)
{
    std::string username = msg.value("username", "");
    std::string message = msg.value("message", "");

    if (message.empty()) return;

    server_->getChatManager().broadcastLobby(userId, username, message);
}

// ========== 房间聊天 ==========
void GameWsHandler::handleChatRoom(const TcpConnectionPtr& conn, int userId,
                                    const nlohmann::json& msg)
{
    int roomId = server_->getRoomByUserId(userId);
    if (roomId <= 0) return;

    auto room = server_->getGameRoom(roomId);
    if (!room) return;

    std::string username = msg.value("username", "");
    std::string message = msg.value("message", "");

    if (message.empty()) return;

    int opponentId = room->getOpponent(userId);
    auto oppConn = getConnectionByUserId(opponentId);

    server_->getChatManager().sendToRoom(roomId, userId, username, message, conn, oppConn);
}

// ========== 离开游戏 ==========
void GameWsHandler::handleLeaveGame(int userId)
{
    int roomId = server_->getRoomByUserId(userId);
    if (roomId <= 0) return;

    auto room = server_->getGameRoom(roomId);
    if (!room) return;

    int opponentId = room->getOpponent(userId);

    // 对手获胜
    if (server_->finishGameRoom(roomId, opponentId).status != PvpGameStatus::kOk)
    {
        LOG_WARN << "Failed to finish room after leave: " << roomId;
        return;
    }

    auto& wsServer = server_->getHttpServer().getWsServer();

    // 通知对手
    auto oppConn = getConnectionByUserId(opponentId);
    if (oppConn && oppConn->connected())
    {
        json gameOver;
        gameOver["type"] = "game_over";
        gameOver["winner"] = opponentId;
        gameOver["reason"] = "opponent_left";
        gameOver["message"] = "对手已离开，你获胜了！";
        wsServer.sendMessage(oppConn, gameOver.dump());

        // 将对手移回大厅
        server_->getChatManager().addToLobby(opponentId, oppConn);
    }

    // 将当前用户移回大厅
    auto conn = getConnectionByUserId(userId);
    if (conn && conn->connected())
    {
        server_->getChatManager().addToLobby(userId, conn);
    }

    // 清理房间
    server_->removeGameRoom(roomId);
}

// ========== 连接 <-> 用户映射 ==========
int GameWsHandler::getUserId(const TcpConnectionPtr& conn) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connToUserId_.find(conn->name());
    return (it != connToUserId_.end()) ? it->second : 0;
}

void GameWsHandler::setUserId(const TcpConnectionPtr& conn, int userId)
{
    std::string connName = conn->name();
    std::lock_guard<std::mutex> lock(mutex_);
    connToUserId_[connName] = userId;
    userIdToConnPtr_[userId] = conn;
}

void GameWsHandler::removeUserId(const TcpConnectionPtr& conn)
{
    std::string connName = conn->name();
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connToUserId_.find(connName);
    if (it != connToUserId_.end())
    {
        int userId = it->second;
        userIdToConnPtr_.erase(userId);
        connToUserId_.erase(it);
    }
}

GameWsHandler::TcpConnectionPtr GameWsHandler::getConnectionByUserId(int userId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = userIdToConnPtr_.find(userId);
    return (it != userIdToConnPtr_.end()) ? it->second : TcpConnectionPtr();
}
