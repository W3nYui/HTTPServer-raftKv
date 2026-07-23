#include "PvpGameService.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

PvpGameService::PvpGameService(GameStateStore& store)
    : store_(store)
{
}

PvpGameResult PvpGameService::createRoom(int player1, int player2)
{
    std::lock_guard<std::mutex> lock(writerMutex_);

    const auto next = store_.load(nextRoomIdKey());
    if (next.status == GameStateStoreStatus::kUnavailable) return {PvpGameStatus::kUnavailable, {}};

    const int roomId = next.status == GameStateStoreStatus::kOk ? next.value.get<int>() : 1;
    GameRoom room(roomId, player1, player2);
    const auto snapshot = room.snapshot();

    const auto active = store_.load(activeRoomIdsKey());
    if (active.status == GameStateStoreStatus::kUnavailable) return {PvpGameStatus::kUnavailable, {}};

    if (active.status == GameStateStoreStatus::kOk && !active.value.is_array())
        return {PvpGameStatus::kUnavailable, {}};
    auto activeRoomIds = active.status == GameStateStoreStatus::kOk ? active.value : GameRoom::Json::array();
    activeRoomIds.push_back(roomId);
    if (store_.replace({{roomKey(roomId), snapshot},
                        {activeRoomIdsKey(), activeRoomIds},
                        {nextRoomIdKey(), roomId + 1}}) != GameStateStoreStatus::kOk)
        return {PvpGameStatus::kUnavailable, {}};

    return {PvpGameStatus::kOk, snapshot};
}

PvpGameResult PvpGameService::load(int roomId)
{
    std::lock_guard<std::mutex> lock(writerMutex_);
    return loadLocked(roomId);
}

PvpGameResult PvpGameService::move(int roomId, int playerId, int x, int y)
{
    std::lock_guard<std::mutex> lock(writerMutex_);
    const auto                  current = loadLocked(roomId);
    if (current.status != PvpGameStatus::kOk) return current;

    std::shared_ptr<GameRoom> room;
    try
    {
        room = GameRoom::fromSnapshot(current.snapshot);
    }
    catch (const std::exception&)
    {
        return {PvpGameStatus::kUnavailable, {}};
    }

    const int moveResult = room->makeMove(playerId, x, y);
    if (moveResult == -1) return {PvpGameStatus::kInvalidMove, current.snapshot};
    if (moveResult == -2) return {PvpGameStatus::kGameOver, current.snapshot};
    if (moveResult == -3) return {PvpGameStatus::kNotYourTurn, current.snapshot};

    const auto replacement = room->snapshot();
    if (!replacement.at("gameOver").get<bool>())
    {
        if (store_.replace({{roomKey(roomId), replacement}}) != GameStateStoreStatus::kOk)
            return {PvpGameStatus::kUnavailable, current.snapshot};
        return {PvpGameStatus::kOk, replacement};
    }

    const auto active = store_.load(activeRoomIdsKey());
    if (active.status != GameStateStoreStatus::kOk || !active.value.is_array())
        return {PvpGameStatus::kUnavailable, current.snapshot};

    auto activeRoomIds = active.value;
    activeRoomIds.erase(std::remove(activeRoomIds.begin(), activeRoomIds.end(), roomId), activeRoomIds.end());
    if (store_.replace({{roomKey(roomId), replacement}, {activeRoomIdsKey(), activeRoomIds}}) !=
        GameStateStoreStatus::kOk)
        return {PvpGameStatus::kUnavailable, current.snapshot};
    return {PvpGameStatus::kOk, replacement};
}

PvpGameResult PvpGameService::finish(int roomId, int winnerId)
{
    std::lock_guard<std::mutex> lock(writerMutex_);
    const auto                  current = loadLocked(roomId);
    if (current.status != PvpGameStatus::kOk) return current;

    std::shared_ptr<GameRoom> room;
    try
    {
        room = GameRoom::fromSnapshot(current.snapshot);
    }
    catch (const std::exception&)
    {
        return {PvpGameStatus::kUnavailable, {}};
    }

    if (!room->isGameOver()) room->forfeit(winnerId);
    const auto replacement = room->snapshot();

    const auto active = store_.load(activeRoomIdsKey());
    if (active.status != GameStateStoreStatus::kOk || !active.value.is_array())
        return {PvpGameStatus::kUnavailable, current.snapshot};

    auto activeRoomIds = active.value;
    activeRoomIds.erase(std::remove(activeRoomIds.begin(), activeRoomIds.end(), roomId), activeRoomIds.end());
    if (store_.replace({{roomKey(roomId), replacement}, {activeRoomIdsKey(), activeRoomIds}}) !=
        GameStateStoreStatus::kOk)
        return {PvpGameStatus::kUnavailable, current.snapshot};
    return {PvpGameStatus::kOk, replacement};
}

std::vector<GameRoom::Json> PvpGameService::recoverActiveRooms()
{
    std::lock_guard<std::mutex> lock(writerMutex_);
    const auto                  active = store_.load(activeRoomIdsKey());
    if (active.status != GameStateStoreStatus::kOk || !active.value.is_array()) return {};

    std::vector<GameRoom::Json> rooms;
    try
    {
        for (const auto& roomId : active.value)
        {
            const auto current = loadLocked(roomId.get<int>());
            if (current.status == PvpGameStatus::kOk && !current.snapshot.at("gameOver").get<bool>())
                rooms.push_back(current.snapshot);
        }
    }
    catch (const std::exception&)
    {
        return {};
    }
    return rooms;
}

std::string PvpGameService::roomKey(int roomId)
{
    return "gomoku:room:" + std::to_string(roomId);
}

const std::string& PvpGameService::activeRoomIdsKey()
{
    static const std::string key = "gomoku:active-room-ids";
    return key;
}

const std::string& PvpGameService::nextRoomIdKey()
{
    static const std::string key = "gomoku:next-room-id";
    return key;
}

PvpGameResult PvpGameService::loadLocked(int roomId)
{
    const auto stored = store_.load(roomKey(roomId));
    if (stored.status == GameStateStoreStatus::kNotFound) return {PvpGameStatus::kNotFound, {}};
    if (stored.status != GameStateStoreStatus::kOk) return {PvpGameStatus::kUnavailable, {}};
    return {PvpGameStatus::kOk, stored.value};
}
