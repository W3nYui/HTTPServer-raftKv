#include "RaftGameStateStore.h"
#include "PvpGameService.h"

#include <cassert>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

class FakeRaftClient : public RaftGameStateClient
{
public:
    RaftGameStateReadResult get(const std::string& key, std::chrono::milliseconds timeout) override
    {
        lastDeadline = timeout;
        if (readsUnavailable) return {RaftGameStateClientStatus::kUnavailable, {}};
        const auto value = values.find(key);
        if (value == values.end()) return {RaftGameStateClientStatus::kNotFound, {}};
        return {RaftGameStateClientStatus::kOk, value->second};
    }

    RaftGameStateClientStatus put(const std::string& key,
                                  const std::string& value,
                                  std::chrono::milliseconds timeout) override
    {
        lastDeadline = timeout;
        ++putCalls;
        if (nextPutStatus != RaftGameStateClientStatus::kOk ||
            (failAfterSuccessfulPuts && successfulPuts == *failAfterSuccessfulPuts))
            return RaftGameStateClientStatus::kUnavailable;
        values[key] = value;
        ++successfulPuts;
        return RaftGameStateClientStatus::kOk;
    }

    std::chrono::milliseconds                        lastDeadline{0};
    RaftGameStateClientStatus                         nextPutStatus = RaftGameStateClientStatus::kOk;
    std::unordered_map<std::string, std::string>      values;
    bool                                              readsUnavailable = false;
    std::optional<std::size_t>                         failAfterSuccessfulPuts;
    std::size_t                                        putCalls = 0;
    std::size_t                                        successfulPuts = 0;
};

int main()
{
    FakeRaftClient client;
    RaftGameStateStore store(std::shared_ptr<RaftGameStateClient>(&client, [](RaftGameStateClient*) {}),
                             std::chrono::milliseconds(500));

    client.values["gomoku:state"] = R"({"gomoku:room:1":{"roomId":1}})";
    const auto loaded = store.load("gomoku:room:1");
    assert(loaded.status == GameStateStoreStatus::kOk);
    assert(loaded.value.at("roomId") == 1);
    assert(client.lastDeadline == std::chrono::milliseconds(500));

    client.nextPutStatus = RaftGameStateClientStatus::kUnavailable;
    assert(store.replace({{"gomoku:room:1", {{"roomId", 2}}}}) == GameStateStoreStatus::kUnavailable);
    assert(client.lastDeadline == std::chrono::milliseconds(500));
    assert(client.values.at("gomoku:state") == R"({"gomoku:room:1":{"roomId":1}})");

    client.nextPutStatus = RaftGameStateClientStatus::kOk;
    client.values["gomoku:state"] = "not-json";
    assert(store.load("gomoku:room:2").status == GameStateStoreStatus::kUnavailable);

    auto persistentClient = std::make_shared<FakeRaftClient>();
    persistentClient->failAfterSuccessfulPuts = 1;
    RaftGameStateStore firstStore(persistentClient, std::chrono::milliseconds(500));
    PvpGameService firstService(firstStore);
    const auto created = firstService.createRoom(11, 22);
    assert(created.status == PvpGameStatus::kOk);
    assert(persistentClient->putCalls == 1);

    RaftGameStateStore restartedStore(persistentClient, std::chrono::milliseconds(500));
    PvpGameService restartedService(restartedStore);
    assert(restartedService.recoverActiveRooms() == std::vector<GameRoom::Json>{created.snapshot});

    persistentClient->failAfterSuccessfulPuts = persistentClient->successfulPuts;
    assert(firstService.move(created.snapshot.at("roomId"), 11, 3, 4).status == PvpGameStatus::kUnavailable);
    persistentClient->failAfterSuccessfulPuts.reset();
    assert(restartedService.recoverActiveRooms() == std::vector<GameRoom::Json>{created.snapshot});

    const auto finished = firstService.finish(created.snapshot.at("roomId"), 11);
    assert(finished.status == PvpGameStatus::kOk);
    const auto envelope = nlohmann::json::parse(persistentClient->values.at("gomoku:state"));
    assert(envelope.at("gomoku:room:1").at("gameOver") == true);
    assert(envelope.at("gomoku:active-room-ids") == nlohmann::json::array());
    assert(restartedService.recoverActiveRooms().empty());

    persistentClient->readsUnavailable = true;
    RaftGameStateStore unavailableStore(persistentClient, std::chrono::milliseconds(500));
    PvpGameService unavailableService(unavailableStore);
    assert(unavailableService.recoverActiveRooms().empty());
}
