#include "RaftGameStateStore.h"

#include <utility>

namespace
{

const std::string& stateEnvelopeKey()
{
    static const std::string key = "gomoku:state";
    return key;
}

bool isStateEnvelope(const nlohmann::json& value)
{
    return value.is_object();
}

} // namespace

RaftGameStateStore::RaftGameStateStore(std::shared_ptr<RaftGameStateClient> client,
                                       std::chrono::milliseconds deadline)
    : client_(std::move(client)), deadline_(deadline)
{
}

GameStateStoreReadResult RaftGameStateStore::load(const std::string& key)
{
    const auto result = client_->get(stateEnvelopeKey(), deadline_);
    if (result.status == RaftGameStateClientStatus::kNotFound) return {GameStateStoreStatus::kNotFound, {}};
    if (result.status != RaftGameStateClientStatus::kOk) return {GameStateStoreStatus::kUnavailable, {}};

    try
    {
        const auto envelope = nlohmann::json::parse(result.value);
        if (!isStateEnvelope(envelope)) return {GameStateStoreStatus::kUnavailable, {}};
        const auto value = envelope.find(key);
        if (value == envelope.end()) return {GameStateStoreStatus::kNotFound, {}};
        return {GameStateStoreStatus::kOk, *value};
    }
    catch (const nlohmann::json::exception&)
    {
        return {GameStateStoreStatus::kUnavailable, {}};
    }
}

GameStateStoreStatus RaftGameStateStore::replace(const Records& records)
{
    nlohmann::json envelope;
    const auto stored = client_->get(stateEnvelopeKey(), deadline_);
    if (stored.status == RaftGameStateClientStatus::kNotFound)
    {
        envelope = nlohmann::json::object();
    }
    else if (stored.status != RaftGameStateClientStatus::kOk)
    {
        return GameStateStoreStatus::kUnavailable;
    }
    else
    {
        try
        {
            envelope = nlohmann::json::parse(stored.value);
        }
        catch (const nlohmann::json::exception&)
        {
            return GameStateStoreStatus::kUnavailable;
        }
        if (!isStateEnvelope(envelope)) return GameStateStoreStatus::kUnavailable;
    }

    for (const auto& [key, value] : records)
    {
        envelope[key] = value;
    }
    return client_->put(stateEnvelopeKey(), envelope.dump(), deadline_) == RaftGameStateClientStatus::kOk
               ? GameStateStoreStatus::kOk
               : GameStateStoreStatus::kUnavailable;
}
