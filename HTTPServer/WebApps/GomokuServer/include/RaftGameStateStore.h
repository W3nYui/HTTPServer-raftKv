#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "GameStateStore.h"

class Clerk;

enum class RaftGameStateClientStatus
{
    kOk,
    kNotFound,
    kUnavailable,
};

struct RaftGameStateReadResult
{
    RaftGameStateClientStatus status;
    std::string               value;
};

class RaftGameStateClient
{
public:
    virtual ~RaftGameStateClient() = default;

    virtual RaftGameStateReadResult get(const std::string& key, std::chrono::milliseconds timeout) = 0;
    virtual RaftGameStateClientStatus put(const std::string& key,
                                          const std::string& value,
                                          std::chrono::milliseconds timeout) = 0;
};

class ClerkRaftGameStateClient : public RaftGameStateClient
{
public:
    explicit ClerkRaftGameStateClient(const std::string& configPath);
    ~ClerkRaftGameStateClient() override;

    RaftGameStateReadResult get(const std::string& key, std::chrono::milliseconds timeout) override;
    RaftGameStateClientStatus put(const std::string& key,
                                  const std::string& value,
                                  std::chrono::milliseconds timeout) override;

private:
    std::unique_ptr<Clerk> clerk_;
};

class RaftGameStateStore : public GameStateStore
{
public:
    explicit RaftGameStateStore(std::shared_ptr<RaftGameStateClient> client,
                                std::chrono::milliseconds deadline = std::chrono::milliseconds(500));

    GameStateStoreReadResult load(const std::string& key) override;
    GameStateStoreStatus replace(const Records& records) override;

private:
    std::shared_ptr<RaftGameStateClient> client_;
    std::chrono::milliseconds            deadline_;
};
