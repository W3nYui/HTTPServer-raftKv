#include "gameWsMessages.h"

GameWsDelivery GameWsMessages::raftUnavailable(int roomId)
{
    return {{{"type", "raft_unavailable"}, {"roomId", roomId}}, false};
}

nlohmann::json GameWsMessages::stateResult(const nlohmann::json& snapshot)
{
    return {{"type", "state_result"}, {"state", snapshot}};
}

GameWsDelivery GameWsMessages::moveResult(int roomId,
                                           int playerId,
                                           int x,
                                           int y,
                                           const std::string& color,
                                           const nlohmann::json& snapshot)
{
    return {{{"type", "move_result"},
             {"roomId", roomId},
             {"x", x},
             {"y", y},
             {"color", color},
             {"userId", playerId},
             {"state", snapshot}},
            true};
}
