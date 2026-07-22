#include "../include/MatchmakingPool.h"

#include <algorithm>

int MatchmakingPool::tryMatch(int userId)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已在队列中
    auto it = std::find(waitingQueue_.begin(), waitingQueue_.end(), userId);
    if (it != waitingQueue_.end())
    {
        return -2; // 已在队列中
    }

    // 队列中有等待的玩家，直接配对
    if (!waitingQueue_.empty())
    {
        int opponent = waitingQueue_.front();
        waitingQueue_.pop_front();
        return opponent; // 返回对手 ID
    }

    // 队列为空，加入等待
    waitingQueue_.push_back(userId);
    return -1; // 等待匹配中
}

void MatchmakingPool::leaveQueue(int userId)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find(waitingQueue_.begin(), waitingQueue_.end(), userId);
    if (it != waitingQueue_.end())
    {
        waitingQueue_.erase(it);
    }
}

bool MatchmakingPool::isInQueue(int userId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return std::find(waitingQueue_.begin(), waitingQueue_.end(), userId) != waitingQueue_.end();
}
