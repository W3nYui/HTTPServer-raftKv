#include "../include/ChatManager.h"
#include "../../../HttpServer/include/websocket/WebSocketFrame.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"

#include <chrono>

#include <muduo/base/Logging.h>

using namespace http::websocket;
using json = nlohmann::json;

// ========== 大厅管理 ==========
void ChatManager::addToLobby(int userId, const TcpConnectionPtr& conn)
{
    std::lock_guard<std::mutex> lock(mutex_);
    lobbyConnections_[userId] = conn;
    LOG_INFO << "User " << userId << " joined lobby chat";
}

void ChatManager::removeFromLobby(int userId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    lobbyConnections_.erase(userId);
    LOG_INFO << "User " << userId << " left lobby chat";
}

// ========== 获取当前时间戳 ==========
static long long getNowTimestamp()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch()).count();
}

// ========== 大厅广播 ==========
void ChatManager::broadcastLobby(int senderId, const std::string& username,
                                  const std::string& message)
{
    json msg;
    msg["type"] = "lobby_broadcast";
    msg["userId"] = senderId;
    msg["username"] = username;
    msg["message"] = message;
    msg["timestamp"] = getNowTimestamp();
    std::string jsonStr = msg.dump();

    LOG_INFO << "Lobby broadcast from user " << senderId << ": " << message;

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [userId, conn] : lobbyConnections_)
    {
        if (conn && conn->connected())
        {
            sendJsonMessage(conn, jsonStr);
        }
    }
}

// ========== 房间聊天 ==========
void ChatManager::sendToRoom(int roomId, int senderId, const std::string& username,
                              const std::string& message,
                              const TcpConnectionPtr& conn1,
                              const TcpConnectionPtr& conn2)
{
    json msg;
    msg["type"] = "room_message";
    msg["roomId"] = roomId;
    msg["userId"] = senderId;
    msg["username"] = username;
    msg["message"] = message;
    msg["timestamp"] = getNowTimestamp();
    std::string jsonStr = msg.dump();

    if (conn1 && conn1->connected()) sendJsonMessage(conn1, jsonStr);
    if (conn2 && conn2->connected()) sendJsonMessage(conn2, jsonStr);
}

// ========== 系统消息 ==========
void ChatManager::sendSystemMessage(const TcpConnectionPtr& conn, const std::string& message)
{
    json msg;
    msg["type"] = "system_message";
    msg["message"] = message;
    msg["timestamp"] = getNowTimestamp();
    std::string jsonStr = msg.dump();

    sendJsonMessage(conn, jsonStr);
}

// ========== 获取大厅人数 ==========
size_t ChatManager::getLobbyCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return lobbyConnections_.size();
}

// ========== 底层发送 ==========
void ChatManager::sendJsonMessage(const TcpConnectionPtr& conn, const std::string& jsonStr)
{
    auto frame = WebSocketFrame::createFrame(WebSocketOpCode::Text, jsonStr, false);
    std::string wireData = frame.encodeToString();
    conn->send(wireData);
}
