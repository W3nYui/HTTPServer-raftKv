#include "gameWsMessages.h"
#include "gameWsMoveFlow.h"
#include "GameStateStore.h"
#include "PvpGameService.h"

#include <cassert>
#include <vector>

int main()
{
    const nlohmann::json snapshot = {
        {"roomId", 7},
        {"currentTurn", 22},
        {"board", {{{"black"}}}},
    };

    const auto unavailable = GameWsMessages::raftUnavailable(7);
    assert(!unavailable.broadcast);
    const nlohmann::json expectedUnavailable = {{"type", "raft_unavailable"}, {"roomId", 7}};
    assert(unavailable.message == expectedUnavailable);

    const auto state = GameWsMessages::stateResult(snapshot);
    assert(state.at("type") == "state_result");
    assert(state.at("state") == snapshot);

    const auto moved = GameWsMessages::moveResult(7, 11, 3, 4, "black", snapshot);
    assert(moved.broadcast);
    assert(moved.message.at("type") == "move_result");
    assert(moved.message.at("roomId") == 7);
    assert(moved.message.at("userId") == 11);
    assert(moved.message.at("x") == 3);
    assert(moved.message.at("y") == 4);
    assert(moved.message.at("color") == "black");
    assert(moved.message.at("state") == snapshot);

    MemoryGameStateStore store;
    PvpGameService service(store);
    const auto created = service.createRoom(11, 22);
    const int roomId = created.snapshot.at("roomId").get<int>();
    const auto committed = service.load(roomId).snapshot;

    store.failNextWrite();
    std::vector<nlohmann::json> requesterMessages;
    const auto result = GameWsMoveFlow::submit(
        [&] { return service.move(roomId, 11, 4, 4); },
        roomId,
        [&](const nlohmann::json& message) { requesterMessages.push_back(message); });
    assert(result.status == PvpGameStatus::kUnavailable);
    assert(requesterMessages.size() == 1);
    const nlohmann::json expectedFailedMove = {{"type", "raft_unavailable"}, {"roomId", roomId}};
    assert(requesterMessages.front() == expectedFailedMove);
    assert(service.load(roomId).snapshot == committed);
    assert(service.load(roomId).snapshot.at("board").at(4).at(4) == "empty");
}
