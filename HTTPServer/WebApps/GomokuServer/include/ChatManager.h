#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <muduo/net/TcpConnection.h>

/**
 * @brief 聊天管理器
 *
 * 管理两类聊天频道：
 *   1. 大厅聊天（lobby）：消息广播给所有在大厅的在线用户
 *   2. 房间聊天（room）：消息仅发送给同一房间的两名玩家
 */
class ChatManager
{
public:
    using TcpConnectionPtr = muduo::net::TcpConnectionPtr;
    using MessageSender = std::function<void(const TcpConnectionPtr&, const std::string&)>;

    ChatManager() = default;

    void setMessageSender(MessageSender sender)
    {
        messageSender_ = std::move(sender);
    }

    // ========== 大厅管理 ==========

    /**
     * @brief 添加用户到大厅（上线后调用）
     * @param userId 用户 ID
     * @param conn   WebSocket 连接
     */
    void addToLobby(int userId, const TcpConnectionPtr& conn);

    /**
     * @brief 从大厅移除用户（下线或进入房间后调用）
     * @param userId 用户 ID
     */
    void removeFromLobby(int userId);

    // ========== 消息发送 ==========

    /**
     * @brief 大厅聊天广播
     * @param senderId 发送者 ID
     * @param username 发送者用户名
     * @param message  消息内容
     */
    void broadcastLobby(int senderId, const std::string& username, const std::string& message);

    /**
     * @brief 房间聊天
     * @param roomId   房间 ID
     * @param senderId 发送者 ID
     * @param username 发送者用户名
     * @param message  消息内容
     * @param conn1    玩家1的 WebSocket 连接
     * @param conn2    玩家2的 WebSocket 连接
     */
    void sendToRoom(int roomId, int senderId, const std::string& username,
                    const std::string& message,
                    const TcpConnectionPtr& conn1,
                    const TcpConnectionPtr& conn2);

    /**
     * @brief 发送系统消息给单个用户
     */
    void sendSystemMessage(const TcpConnectionPtr& conn, const std::string& message);

    /**
     * @brief 获取在线大厅连接数
     */
    size_t getLobbyCount() const;

private:
    /**
     * @brief 发送 WebSocket 文本消息（序列化为 JSON 后发送）
     */
    void sendJsonMessage(const TcpConnectionPtr& conn, const std::string& jsonStr) const;

    // userId -> WebSocket 连接（大厅在线用户）
    std::unordered_map<int, TcpConnectionPtr> lobbyConnections_;
    mutable std::mutex mutex_;
    MessageSender messageSender_;
};
