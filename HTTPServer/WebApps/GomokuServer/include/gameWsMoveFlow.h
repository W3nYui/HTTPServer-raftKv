#pragma once

#include <functional>

#include "PvpGameService.h"

class GameWsMoveFlow
{
public:
    using CommitMove = std::function<PvpGameResult()>;
    using SendToRequester = std::function<void(const nlohmann::json&)>;

    static PvpGameResult submit(CommitMove commitMove,
                                int roomId,
                                const SendToRequester& sendToRequester);
};
