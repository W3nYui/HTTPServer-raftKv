#include "gameWsMoveFlow.h"

#include "gameWsMessages.h"

PvpGameResult GameWsMoveFlow::submit(CommitMove commitMove,
                                     int roomId,
                                     const SendToRequester& sendToRequester)
{
    const auto result = commitMove();
    if (result.status != PvpGameStatus::kUnavailable) return result;

    const auto delivery = GameWsMessages::raftUnavailable(roomId);
    if (!delivery.broadcast) sendToRequester(delivery.message);
    return result;
}
