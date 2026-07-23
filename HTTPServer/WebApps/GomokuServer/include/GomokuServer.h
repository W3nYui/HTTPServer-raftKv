#pragma once

#include <atomic>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <mutex>


#include "AiGame.h"
#include "GameRoom.h"
#include "GameStateStore.h"
#include "MatchmakingPool.h"
#include "PvpGameService.h"
#include "ChatManager.h"
#include "../../../HttpServer/include/http/HttpServer.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../../../HttpServer/include/utils/FileUtil.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"


class LoginHandler;
class EntryHandler;
class RegisterHandler;
class MenuHandler;
class AiGameStartHandler;
class LogoutHandler;
class AiGameMoveHandler;
class GameBackendHandler;
class GameWsHandler;

#define DURING_GAME 1 
#define GAME_OVER 2

#define MAX_AIBOT_NUM 4096

class GomokuServer
{
public:
    GomokuServer(int port,
                 const std::string& name,
                 bool useSSL,
                 muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort); // 默认不复用端口

    void setThreadNum(int numThreads);
    void start();
private:
    void initialize();
    void initializeSession();
    void initializeRouter();
    void initializeMiddleware();
    
    void setSessionManager(std::unique_ptr<http::session::SessionManager> manager)
    {
        httpServer_.setSessionManager(std::move(manager));
    }

    http::session::SessionManager*  getSessionManager() const
    {
        return httpServer_.getSessionManager();
    }
    
    void restartChessGameVsAi(const http::HttpRequest& req, http::HttpResponse* resp);
    void getBackendData(const http::HttpRequest& req, http::HttpResponse* resp);

    void packageResp(const std::string& version, http::HttpResponse::HttpStatusCode statusCode,
                     const std::string& statusMsg, bool close, const std::string& contentType,
                     int contentLen, const std::string& body, http::HttpResponse* resp);

    // 获取历史最高在线人数
    int getMaxOnline() const
    {
        return maxOnline_.load();
    }

    // 获取当前在线人数
    int getCurOnline() const
    {
        return onlineUsers_.size();
    }

    void updateMaxOnline(int online)
    {
        maxOnline_ = std::max(maxOnline_.load(), online);
    }

    // 获取用户总数
    int getUserCount()
    {
        std::string sql = "SELECT COUNT(*) as count FROM users";

        sql::ResultSet* res = mysqlUtil_.executeQuery(sql);
        if (res->next())
        {
            return res->getInt("count");
        }
        return 0;
    }

    // ========== WebSocket / PVP / 聊天 对外接口 ==========

    /**
     * @brief 获取底层 HttpServer（用于访问 WebSocketServer）
     */
    http::HttpServer& getHttpServer()
    {
        return httpServer_;
    }

    /**
     * @brief 获取匹配池
     */
    MatchmakingPool& getMatchmakingPool()
    {
        return matchmakingPool_;
    }

    /**
     * @brief 获取聊天管理器
     */
    ChatManager& getChatManager()
    {
        return chatManager_;
    }

    /**
     * @brief 创建 PVP 对局房间
     * @param player1 黑棋（先手）
     * @param player2 白棋（后手）
     * @return 房间 ID
     */
    int createGameRoom(int player1, int player2);

    /**
     * @brief 获取对局房间
     */
    std::shared_ptr<GameRoom> getGameRoom(int roomId);

    PvpGameResult moveGameRoom(int roomId, int playerId, int x, int y);
    PvpGameResult finishGameRoom(int roomId, int winnerId);

    /**
     * @brief 通过用户 ID 查找其所在的房间号
     * @return 房间号，0 表示不在任何房间
     */
    int getRoomByUserId(int userId) const;

    /**
     * @brief 移除对局房间
     */
    void removeGameRoom(int roomId);

private:
    friend class EntryHandler;
    friend class LoginHandler;
    friend class RegisterHandler;
    friend class MenuHandler;
    friend class AiGameStartHandler;
    friend class LogoutHandler;
    friend class AiGameMoveHandler;
    friend class GameBackendHandler;
    friend class GameWsHandler;

private:
    enum GameType
    {
        NO_GAME = 0,
        MAN_VS_AI = 1,
        MAN_VS_MAN = 2
    };
    // 实际业务制定由GomokuServer来完成
    // 需要留意httpServer_提供哪些接口供使用
    http::HttpServer                                 httpServer_;
    http::MysqlUtil                                  mysqlUtil_;
    // userId -> AiBot
    std::unordered_map<int, std::shared_ptr<AiGame>> aiGames_;
    std::mutex                                       mutexForAiGames_;
    // userId -> 是否在游戏中
    std::unordered_map<int, bool>                    onlineUsers_;
    std::mutex                                       mutexForOnlineUsers_; 
    // 最高在线人数
    std::atomic<int>                                 maxOnline_;
    // PVP 匹配池
    MatchmakingPool                                  matchmakingPool_;
    // 聊天管理器（大厅聊天 + 房间聊天）
    ChatManager                                      chatManager_;
    std::unique_ptr<MemoryGameStateStore>            gameStateStore_;
    std::unique_ptr<PvpGameService>                  pvpGameService_;
    // roomId -> GameRoom PVP 对局房间
    std::unordered_map<int, std::shared_ptr<GameRoom>> gameRooms_;
    mutable std::mutex                               mutexForGameRooms_;
    // WebSocket 消息处理器
    std::shared_ptr<GameWsHandler>                   wsHandler_;
};
