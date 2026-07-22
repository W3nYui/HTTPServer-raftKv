#include "GameRoom.h"

#include <cassert>
#include <nlohmann/json.hpp>

int main()
{
    GameRoom room(7, 11, 22);
    assert(room.makeMove(11, 7, 7) == 0);

    const auto snapshot = room.snapshot();
    assert(snapshot.at("roomId") == 7);
    assert(snapshot.at("player1") == 11);
    assert(snapshot.at("player2") == 22);
    assert(snapshot.at("board").at(7).at(7) == "black");
    assert(snapshot.at("currentTurn") == 22);

    // Restored rooms must preserve the turn so the next command is validated identically.
    auto restored = GameRoom::fromSnapshot(snapshot);
    assert(restored->makeMove(22, 7, 8) == 0);

    // The legacy string APIs must serialize through the same single-lock snapshot path.
    assert(nlohmann::json::parse(room.getBoardJson()) == snapshot.at("board"));
    assert(nlohmann::json::parse(room.getGameStateJson()) == snapshot);
}
