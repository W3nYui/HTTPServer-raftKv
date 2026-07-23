#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

enum class GameStateStoreStatus
{
    kOk,
    kNotFound,
    kUnavailable,
};

struct GameStateStoreReadResult
{
    GameStateStoreStatus status;
    nlohmann::json      value;
};

class GameStateStore
{
public:
    using Records = std::vector<std::pair<std::string, nlohmann::json>>;

    virtual ~GameStateStore() = default;

    virtual GameStateStoreReadResult load(const std::string& key) = 0;
    virtual GameStateStoreStatus replace(const Records& records) = 0;
};

class MemoryGameStateStore : public GameStateStore
{
public:
    GameStateStoreReadResult load(const std::string& key) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto                  found = values_.find(key);
        if (found == values_.end()) return {GameStateStoreStatus::kNotFound, {}};
        return {GameStateStoreStatus::kOk, found->second};
    }

    GameStateStoreStatus replace(const Records& records) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (failNextWrite_ || (writesBeforeFailure_ && *writesBeforeFailure_ < records.size()))
        {
            failNextWrite_ = false;
            writesBeforeFailure_.reset();
            return GameStateStoreStatus::kUnavailable;
        }
        if (writesBeforeFailure_) *writesBeforeFailure_ -= records.size();
        for (const auto& [key, value] : records)
        {
            values_[key] = value;
        }
        return GameStateStoreStatus::kOk;
    }

    void failNextWrite()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        failNextWrite_ = true;
    }

    void failAfterSuccessfulWrites(std::size_t writes)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        writesBeforeFailure_ = writes;
    }

private:
    std::mutex                                  mutex_;
    std::unordered_map<std::string, nlohmann::json> values_;
    bool                                        failNextWrite_ = false;
    std::optional<std::size_t>                   writesBeforeFailure_;
};
