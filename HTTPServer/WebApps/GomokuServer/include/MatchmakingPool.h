#pragma once

#include <deque>
#include <mutex>

/**
 * @brief 简易匹配池
 *
 * 使用 FIFO 队列实现玩家匹配：
 *   1. 玩家调用 tryMatch() 加入匹配队列
 *   2. 若队列中已有等待玩家，则直接配对（返回对手 ID）
 *   3. 若队列为空，则加入等待，返回 -1
 *   4. 玩家可随时取消匹配
 */
class MatchmakingPool
{
public:
    MatchmakingPool() = default;

    /**
     * @brief 加入匹配队列并尝试匹配
     * @param userId 请求匹配的玩家 ID
     * @return 对手 ID（匹配成功），-1（已加入等待队列），-2（已在队列中）
     */
    int tryMatch(int userId);

    /**
     * @brief 取消匹配
     * @param userId 玩家 ID
     */
    void leaveQueue(int userId);

    /**
     * @brief 判断玩家是否在匹配队列中
     */
    bool isInQueue(int userId) const;

private:
    std::deque<int>      waitingQueue_;  // 等待匹配的玩家队列
    mutable std::mutex   mutex_;         // 线程安全
};
