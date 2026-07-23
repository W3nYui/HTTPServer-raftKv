#pragma once

#include <mutex>
#include <vector>

#include "GameRoom.h"
#include "GameStateStore.h"

enum class PvpGameStatus
{
    kOk,
    kNotFound,
    kInvalidMove,
    kGameOver,
    kNotYourTurn,
    kUnavailable,
};

struct PvpGameResult
{
    PvpGameStatus  status;
    GameRoom::Json snapshot;
};

class PvpGameService
{
public:
    explicit PvpGameService(GameStateStore& store);

    PvpGameResult createRoom(int player1, int player2);
    PvpGameResult load(int roomId);
    PvpGameResult move(int roomId, int playerId, int x, int y);
    PvpGameResult finish(int roomId, int winnerId);
    std::vector<GameRoom::Json> recoverActiveRooms();

private:
    static std::string roomKey(int roomId);
    static const std::string& activeRoomIdsKey();
    static const std::string& nextRoomIdKey();

    PvpGameResult loadLocked(int roomId);
    GameStateStore& store_;
    std::mutex      writerMutex_;
};
