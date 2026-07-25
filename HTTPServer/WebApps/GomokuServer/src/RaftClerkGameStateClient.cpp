#include "RaftGameStateStore.h"

#include <utility>

#include "clerk.h"

namespace
{

RaftGameStateClientStatus toClientStatus(ClerkStatus status)
{
    switch (status)
    {
        case ClerkStatus::kOk: return RaftGameStateClientStatus::kOk;
        case ClerkStatus::kNotFound: return RaftGameStateClientStatus::kNotFound;
        case ClerkStatus::kUnavailable: return RaftGameStateClientStatus::kUnavailable;
    }
    return RaftGameStateClientStatus::kUnavailable;
}

} // namespace

ClerkRaftGameStateClient::ClerkRaftGameStateClient(const std::string& configPath)
    : clerk_(std::make_unique<Clerk>())
{
    clerk_->Init(configPath);
}

ClerkRaftGameStateClient::~ClerkRaftGameStateClient() = default;

RaftGameStateReadResult ClerkRaftGameStateClient::get(const std::string& key, std::chrono::milliseconds timeout)
{
    const auto result = clerk_->TryGet(key, timeout);
    return {toClientStatus(result.status), result.value};
}

RaftGameStateClientStatus ClerkRaftGameStateClient::put(const std::string& key,
                                                        const std::string& value,
                                                        std::chrono::milliseconds timeout)
{
    return toClientStatus(clerk_->TryPut(key, value, timeout));
}
