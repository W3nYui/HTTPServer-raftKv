#include "../include/GameRoom.h"

GameRoom::GameRoom(int roomId, int player1Id, int player2Id)
    : roomId_(roomId)
    , player1_(player1Id)
    , player2_(player2Id)
    , currentTurn_(player1Id)  // 黑棋（player1）先手
    , moveCount_(0)
    , gameOver_(false)
    , winner_(0)
    , winnerReason_("")
    , lastMove_(-1, -1)
    , board_(BOARD_SIZE, std::vector<std::string>(BOARD_SIZE, BOARD_EMPTY))
{
}

// ========== 玩家落子 ==========
int GameRoom::makeMove(int playerId, int x, int y)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 游戏已结束
    if (gameOver_) return -2;

    // 不是你的回合
    if (playerId != currentTurn_) return -3;

    // 检查落子合法性
    if (!isValidMove(x, y)) return -1;

    // 放置棋子
    std::string stoneColor = (playerId == player1_) ? BOARD_BLACK : BOARD_WHITE;
    board_[x][y] = stoneColor;
    moveCount_++;
    lastMove_ = {x, y};

    // 胜负判定
    if (checkWin(x, y, stoneColor))
    {
        gameOver_ = true;
        winner_ = playerId;
        winnerReason_ = "five_in_row";
        return 0;
    }

    // 平局判定
    if (isDraw())
    {
        gameOver_ = true;
        winner_ = -1;
        winnerReason_ = "draw";
        return 0;
    }

    // 切换回合
    currentTurn_ = (currentTurn_ == player1_) ? player2_ : player1_;
    return 0;
}

// ========== 强制结束游戏 ==========
void GameRoom::forfeit(int winnerId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    gameOver_ = true;
    winner_ = winnerId;
    winnerReason_ = "opponent_left";
}

// ========== 胜利判定（与 AiGame::checkWin 逻辑一致） ==========
bool GameRoom::checkWin(int x, int y, const std::string& player)
{
    const int dx[] = {1, 0, 1, 1};
    const int dy[] = {0, 1, 1, -1};

    for (int dir = 0; dir < 4; dir++)
    {
        int count = 1;

        // 正向检查
        for (int i = 1; i < 5; i++)
        {
            int newX = x + dx[dir] * i;
            int newY = y + dy[dir] * i;
            if (!isInBoard(newX, newY) || board_[newX][newY] != player) break;
            count++;
        }

        // 反向检查
        for (int i = 1; i < 5; i++)
        {
            int newX = x - dx[dir] * i;
            int newY = y - dy[dir] * i;
            if (!isInBoard(newX, newY) || board_[newX][newY] != player) break;
            count++;
        }

        if (count >= 5) return true;
    }
    return false;
}

// ========== 辅助方法 ==========
bool GameRoom::isValidMove(int x, int y) const
{
    if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) return false;
    if (board_[x][y] != BOARD_EMPTY) return false;
    return true;
}

bool GameRoom::isDraw() const
{
    return moveCount_ >= BOARD_SIZE * BOARD_SIZE;
}

bool GameRoom::isInBoard(int x, int y) const
{
    return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE;
}

// ========== JSON 序列化 ==========
GameRoom::Json GameRoom::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Copy every persisted field while holding one lock so a move cannot split the snapshot.
    return Json{
        {"roomId", roomId_},
        {"player1", player1_},
        {"player2", player2_},
        {"currentTurn", currentTurn_},
        {"moveCount", moveCount_},
        {"gameOver", gameOver_},
        {"winner", winner_},
        {"winnerReason", winnerReason_},
        {"lastMove", {{"x", lastMove_.first}, {"y", lastMove_.second}}},
        {"board", board_},
    };
}

std::shared_ptr<GameRoom> GameRoom::fromSnapshot(const Json& snapshot)
{
    auto room = std::make_shared<GameRoom>(snapshot.at("roomId").get<int>(),
                                           snapshot.at("player1").get<int>(),
                                           snapshot.at("player2").get<int>());

    std::lock_guard<std::mutex> lock(room->mutex_);
    // Restore the complete committed state rather than replaying moves during application recovery.
    room->currentTurn_ = snapshot.at("currentTurn").get<int>();
    room->moveCount_ = snapshot.at("moveCount").get<int>();
    room->gameOver_ = snapshot.at("gameOver").get<bool>();
    room->winner_ = snapshot.at("winner").get<int>();
    room->winnerReason_ = snapshot.at("winnerReason").get<std::string>();
    room->lastMove_ = {snapshot.at("lastMove").at("x").get<int>(), snapshot.at("lastMove").at("y").get<int>()};
    room->board_ = snapshot.at("board").get<std::vector<std::vector<std::string>>>();
    return room;
}

std::string GameRoom::getBoardJson() const
{
    return snapshot().at("board").dump();
}

std::string GameRoom::getGameStateJson() const
{
    return snapshot().dump();
}
