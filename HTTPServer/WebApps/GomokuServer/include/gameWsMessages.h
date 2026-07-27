#pragma once

#include <string>

#include <nlohmann/json.hpp>

struct GameWsDelivery
{
    nlohmann::json message;
    bool           broadcast;
};

class GameWsMessages
{
public:
    static GameWsDelivery raftUnavailable(int roomId);
    static nlohmann::json stateResult(const nlohmann::json& snapshot);
    static GameWsDelivery moveResult(int roomId,
                                     int playerId,
                                     int x,
                                     int y,
                                     const std::string& color,
                                     const nlohmann::json& snapshot);
};
