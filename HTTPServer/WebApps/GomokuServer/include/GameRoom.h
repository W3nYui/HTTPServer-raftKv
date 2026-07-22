#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief PVP 对局房间
 *
 * 每个房间包含两名玩家（player1=黑棋先手, player2=白棋后手），
 * 维护 15x15 棋盘状态，处理落子、胜负判定、平局判定。
 * 棋盘逻辑与 AiGame 一致，移除 AI 部分，改为纯双人对战。
 */
class GameRoom
{
public:
    using Json = nlohmann::json;

    static const int BOARD_SIZE = 15;

    GameRoom(int roomId, int player1Id, int player2Id);

    // ========== 基础信息 ==========
    int roomId()     const { return roomId_; }
    int player1()    const { return player1_; }  // 黑棋（先手）
    int player2()    const { return player2_; }  // 白棋（后手）
    int currentTurn() const { return currentTurn_; }

    int getOpponent(int playerId) const
    {
        return (playerId == player1_) ? player2_ : player1_;
    }

    /**
     * @brief 获取玩家的棋子颜色
     */
    std::string getPlayerColor(int playerId) const
    {
        return (playerId == player1_) ? "black" : "white";
    }

    // ========== 游戏操作 ==========

    /**
     * @brief 玩家落子
     * @param playerId 落子玩家 ID
     * @param x        列坐标 (0-14)
     * @param y        行坐标 (0-14)
     * @return 0=成功, -1=非法移动, -2=游戏已结束, -3=不是你的回合
     */
    int makeMove(int playerId, int x, int y);

    /**
     * @brief 强制结束游戏（对手掉线等）
     * @param winnerId 获胜方玩家 ID
     */
    void forfeit(int winnerId);

    // ========== 状态查询 ==========
    bool isGameOver() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return gameOver_;
    }

    int getWinner() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return winner_;
    }

    std::string getWinnerReason() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return winnerReason_;
    }

    /**
     * @brief 获取当前棋盘状态的 JSON 字符串
     */
    std::string getBoardJson() const;

    /**
     * @brief 获取完整游戏状态的 JSON 字符串
     */
    std::string getGameStateJson() const;

    /**
     * @brief 获取可持久化的完整对局快照
     *
     * 一个快照对应某一时刻的完整状态，因此在同一把锁下读取所有字段，
     * 避免棋盘和回合来自不同的落子前后状态。
     */
    Json snapshot() const;

    /**
     * @brief 从已提交的完整快照恢复对局
     *
     * 仅接受由 snapshot() 写出的字段，调用方应将解析或存储错误作为恢复失败处理。
     */
    static std::shared_ptr<GameRoom> fromSnapshot(const Json& snapshot);

private:
    bool isValidMove(int x, int y) const;
    bool checkWin(int x, int y, const std::string& player);
    bool isDraw() const;
    bool isInBoard(int x, int y) const;

    int                                   roomId_;       // 房间 ID
    int                                   player1_;      // 黑棋（先手）
    int                                   player2_;      // 白棋（后手）
    int                                   currentTurn_;  // 当前轮到谁（player1_ 或 player2_）
    int                                   moveCount_;    // 已落子数

    bool                                  gameOver_;     // 游戏是否结束
    int                                   winner_;       // 获胜方 ID（-1=平局, 0=未结束）
    std::string                           winnerReason_; // 胜利原因

    std::pair<int, int>                   lastMove_;     // 上一步落子位置
    std::vector<std::vector<std::string>> board_;        // 棋盘 [x][y]
    mutable std::mutex                    mutex_;        // 线程安全
};

static const std::string BOARD_EMPTY = "empty";
static const std::string BOARD_BLACK = "black";
static const std::string BOARD_WHITE = "white";
