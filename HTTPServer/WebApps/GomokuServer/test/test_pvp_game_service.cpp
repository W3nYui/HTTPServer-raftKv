#include "GameStateStore.h"
#include "PvpGameService.h"

#include <atomic>
#include <cassert>
#include <thread>

int main()
{
    MemoryGameStateStore store;
    PvpGameService service(store);

    // A late write failure during room creation must not leave a recoverable room behind.
    store.failAfterSuccessfulWrites(1);
    assert(service.createRoom(99, 100).status == PvpGameStatus::kUnavailable);
    assert(service.load(1).status == PvpGameStatus::kNotFound);
    assert(service.recoverActiveRooms().empty());

    const auto created = service.createRoom(11, 22);
    assert(created.status == PvpGameStatus::kOk);
    const int roomId = created.snapshot.at("roomId").get<int>();
    assert(roomId == 1);
    assert(created.snapshot.at("player1") == 11);
    assert(created.snapshot.at("player2") == 22);

    const auto loaded = service.load(roomId);
    assert(loaded.status == PvpGameStatus::kOk);
    assert(loaded.snapshot == created.snapshot);
    assert(service.recoverActiveRooms() == std::vector<GameRoom::Json>{created.snapshot});

    const auto blackMove = service.move(roomId, 11, 3, 4);
    assert(blackMove.status == PvpGameStatus::kOk);
    assert(blackMove.snapshot.at("board").at(3).at(4) == "black");
    assert(blackMove.snapshot.at("currentTurn") == 22);

    // A failed replacement must leave the committed snapshot untouched.
    store.failNextWrite();
    const auto unavailable = service.move(roomId, 22, 3, 5);
    assert(unavailable.status == PvpGameStatus::kUnavailable);
    assert(service.load(roomId).snapshot == blackMove.snapshot);

    const auto whiteMove = service.move(roomId, 22, 3, 5);
    assert(whiteMove.status == PvpGameStatus::kOk);
    assert(whiteMove.snapshot.at("board").at(3).at(5) == "white");

    // A late failure during finish must not end the committed room or change its recovery index.
    store.failAfterSuccessfulWrites(1);
    assert(service.finish(roomId, 11).status == PvpGameStatus::kUnavailable);
    assert(service.load(roomId).snapshot == whiteMove.snapshot);
    assert(service.recoverActiveRooms() == std::vector<GameRoom::Json>{whiteMove.snapshot});

    const auto finished = service.finish(roomId, 11);
    assert(finished.status == PvpGameStatus::kOk);
    assert(finished.snapshot.at("gameOver") == true);
    assert(finished.snapshot.at("winner") == 11);
    assert(service.recoverActiveRooms().empty());

    const auto concurrentRoom = service.createRoom(33, 44);
    const int concurrentRoomId = concurrentRoom.snapshot.at("roomId").get<int>();
    std::atomic<int> committed{0};
    std::thread first([&] {
        if (service.move(concurrentRoomId, 33, 6, 6).status == PvpGameStatus::kOk) ++committed;
    });
    std::thread second([&] {
        if (service.move(concurrentRoomId, 33, 6, 6).status == PvpGameStatus::kOk) ++committed;
    });
    first.join();
    second.join();
    assert(committed == 1);
}
