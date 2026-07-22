#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../../HttpServer/include/websocket/WebSocketHandler.h"
#include "../GomokuServer.h"

/**
 * @brief 游戏 WebSocket 消息处理器
 *
 * 同时继承 RouterHandler（处理 HTTP Upgrade 握手）和
 * WebSocketHandler（处理 WebSocket 帧消息）。
 *
 * 支持的消息类型：
 *   - match_request  / match_cancel   : 匹配池操作
 *   - move                           : PVP 落子
 *   - chat_lobby                     : 大厅聊天
 *   - chat_room                      : 房间聊天
 *   - leave_game                     : 离开房间
 */
class GameWsHandler : public http::router::RouterHandler,
                      public http::websocket::WebSocketHandler,
                      public std::enable_shared_from_this<GameWsHandler>
{
public:
    explicit GameWsHandler(GomokuServer* server);

    // ========== RouterHandler 接口 ==========
    // HTTP 请求到达 /ws 时调用，执行 WebSocket 升级握手
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

    // ========== WebSocketHandler 接口 ==========
    void onOpen(const TcpConnectionPtr& conn) override;
    void onMessage(const TcpConnectionPtr& conn, const std::string& message) override;
    void onClose(const TcpConnectionPtr& conn) override;

    // ========== 连接 -> 用户映射 ==========
    int getUserId(const TcpConnectionPtr& conn) const;
    void setUserId(const TcpConnectionPtr& conn, int userId);
    void removeUserId(const TcpConnectionPtr& conn);
    TcpConnectionPtr getConnectionByUserId(int userId) const;

private:
    // ========== 消息分发 ==========
    void handleMatchRequest(const TcpConnectionPtr& conn, int userId, const nlohmann::json& msg);
    void handleMatchCancel(int userId);
    void handleMove(const TcpConnectionPtr& conn, int userId, const nlohmann::json& msg);
    void handleChatLobby(int userId, const nlohmann::json& msg);
    void handleChatRoom(const TcpConnectionPtr& conn, int userId, const nlohmann::json& msg);
    void handleLeaveGame(int userId);

    GomokuServer* server_;

    // 连接名 -> userId
    std::unordered_map<std::string, int> connToUserId_;
    // userId -> WebSocket 连接
    std::unordered_map<int, TcpConnectionPtr> userIdToConnPtr_;
    mutable std::mutex mutex_;
};
