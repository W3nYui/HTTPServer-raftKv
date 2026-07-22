# Raft Gomoku Integration Implementation Plan

> Required execution discipline: work test-first, run focused checks after each change, and keep each task independently verifiable.

**Goal:** Persist PVP Gomoku state through Raft before broadcasting it, restore active rooms after an HTTP restart, and fix the existing PVP regressions.

**Architecture:** PvpGameService owns the read-validate-write transaction. It reads a GameRoom snapshot through GameStateStore, reconstructs a room, applies one command, persists the replacement, then returns only committed state to GameWsHandler. RaftGameStateStore uses bounded Clerk calls; MemoryGameStateStore supports deterministic unit tests.

**Tech Stack:** C++20, CMake, Muduo, nlohmann/json, Protobuf RPC, Raft KV, Bash, CTest.

---

## Files

| File | Change |
| --- | --- |
| HTTPServer/WebApps/GomokuServer/include/GameRoom.h | Add JSON snapshot and restoration APIs |
| HTTPServer/WebApps/GomokuServer/src/GameRoom.cpp | Serialize under one mutex, no nested locks |
| HTTPServer/WebApps/GomokuServer/include/GameStateStore.h | Define store interface and Raft keys |
| HTTPServer/WebApps/GomokuServer/include/PvpGameService.h | Define create, move, load, finish, recovery commands |
| HTTPServer/WebApps/GomokuServer/src/PvpGameService.cpp | Implement one-writer PVP flow |
| HTTPServer/WebApps/GomokuServer/src/RaftGameStateStore.cpp | Adapt the deadline-aware Clerk |
| HTTPServer/WebApps/GomokuServer/src/GomokuServer.cpp | Create service and recover rooms at startup |
| HTTPServer/WebApps/GomokuServer/src/handlers/GameWsHandler.cpp | Use service and emit committed/failure messages |
| HTTPServer/WebApps/GomokuServer/resource/pvp-client.js | Render committed snapshots and request state refresh |
| raft_KV/src/raftClerk/* | Add bounded Get/Put API |
| raft_KV/src/rpc/mprpcchannel.cpp | Set RPC socket timeouts |
| HTTPServer/CMakeLists.txt, raft_KV/CMakeLists.txt | Add tests and client library linkage |
| scripts/*.sh, README.md | Reproducible build, demonstration, and monorepo check |

### Task 1: Lock Down GameRoom Serialization

**Files:**
- Create: HTTPServer/WebApps/GomokuServer/test/GameRoomTest.cpp
- Modify: HTTPServer/CMakeLists.txt
- Modify: HTTPServer/WebApps/GomokuServer/include/GameRoom.h
- Modify: HTTPServer/WebApps/GomokuServer/src/GameRoom.cpp

- [ ] Write the failing test:

~~~
#include "GameRoom.h"
#include <cassert>

int main() {
  GameRoom room(7, 11, 22);
  assert(room.makeMove(11, 7, 7) == 0);
  const auto snapshot = room.snapshot();
  assert(snapshot.at("roomId") == 7);
  assert(snapshot.at("board").at(7).at(7) == "black");
  assert(snapshot.at("currentTurn") == 22);
  auto restored = GameRoom::fromSnapshot(snapshot);
  assert(restored->makeMove(22, 7, 8) == 0);
}
~~~

- [ ] Add the CTest target, then run:

~~~
enable_testing()
add_executable(game_room_test
  WebApps/GomokuServer/test/GameRoomTest.cpp
  WebApps/GomokuServer/src/GameRoom.cpp)
target_include_directories(game_room_test PRIVATE WebApps/GomokuServer/include)
add_test(NAME game_room_test COMMAND game_room_test)
~~~

Run: cmake -S HTTPServer -B HTTPServer/build && cmake --build HTTPServer/build --target game_room_test && ctest --test-dir HTTPServer/build -R game_room_test --output-on-failure

Expected RED: compile failure because snapshot and fromSnapshot are missing.

- [ ] Implement Json snapshot() const and static std::shared_ptr<GameRoom> fromSnapshot(const Json&). snapshot locks mutex_ once and writes every scalar plus board_ directly. Replace getBoardJson and getGameStateJson with snapshot().dump() wrappers. This fixes the recursive deadlock where getGameStateJson holds mutex_ and calls getBoardJson.

- [ ] Verify GREEN: ctest --test-dir HTTPServer/build -R game_room_test --output-on-failure. Expected: 1/1 test passed.

### Task 2: Bound Raft Clerk Operations

**Files:**
- Create: raft_KV/test/clerk_deadline_test.cpp
- Create: raft_KV/test/unreachable_nodes.conf
- Modify: raft_KV/src/raftClerk/include/clerk.h
- Modify: raft_KV/src/raftClerk/clerk.cpp
- Modify: raft_KV/src/rpc/include/mprpcchannel.h
- Modify: raft_KV/src/rpc/mprpcchannel.cpp
- Modify: raft_KV/CMakeLists.txt

- [ ] Write the failing test:

~~~
Clerk clerk;
clerk.Init("test/unreachable_nodes.conf");
auto began = std::chrono::steady_clock::now();
auto result = clerk.TryGet("missing", std::chrono::milliseconds(250));
assert(result.status == ClerkStatus::kUnavailable);
assert(std::chrono::steady_clock::now() - began < std::chrono::seconds(2));
~~~

- [ ] Run RED: cmake -S raft_KV -B raft_KV/build && cmake --build raft_KV/build --target clerk_deadline_test && timeout 3 raft_KV/bin/clerk_deadline_test

Expected RED: the API is absent; if only the target exists, the existing endless leader loop times out.

- [ ] Add:

~~~
enum class ClerkStatus { kOk, kNotFound, kUnavailable };
struct ClerkGetResult { ClerkStatus status; std::string value; };
ClerkGetResult TryGet(const std::string&, std::chrono::milliseconds);
ClerkStatus TryPut(const std::string&, const std::string&, std::chrono::milliseconds);
~~~

Use a single expiresAt value while cycling leaders. Pass a per-attempt timeout to MprpcChannel and apply SO_SNDTIMEO and SO_RCVTIMEO before connect, send, and recv. Retain existing Get and Put only as compatibility wrappers for existing examples.

- [ ] Add a reusable raft_kv_client static library for the Clerk sources, linked to skip_list_on_raft, Protobuf, Boost serialization, Muduo, pthread, and dl.

- [ ] Verify GREEN: cmake --build raft_KV/build --target clerk_deadline_test && timeout 3 raft_KV/bin/clerk_deadline_test. Expected: exit 0 within two seconds.

### Task 3: Create PVP Persistence Service

**Files:**
- Create: HTTPServer/WebApps/GomokuServer/include/GameStateStore.h
- Create: HTTPServer/WebApps/GomokuServer/include/PvpGameService.h
- Create: HTTPServer/WebApps/GomokuServer/src/PvpGameService.cpp
- Create: HTTPServer/WebApps/GomokuServer/test/PvpGameServiceTest.cpp
- Modify: HTTPServer/CMakeLists.txt

- [ ] Write RED tests with a memory store:

~~~
PvpGameService service(std::make_unique<MemoryGameStateStore>());
auto room = service.createRoom(11, 22);
auto moved = service.move(room.roomId, 11, 3, 4);
assert(moved.status == MoveStatus::kCommitted);
assert(moved.snapshot.at("board").at(3).at(4) == "black");

service.storeForTest().failNextWrite();
auto rejected = service.move(room.roomId, 22, 3, 5);
assert(rejected.status == MoveStatus::kUnavailable);
assert(service.load(room.roomId).at("currentTurn") == 22);
~~~

- [ ] Run RED: cmake --build HTTPServer/build --target pvp_game_service_test && ctest --test-dir HTTPServer/build -R pvp_game_service_test --output-on-failure

Expected RED: target and APIs are absent.

- [ ] Implement GameStateStore::load and ::store, then PvpGameService::createRoom, ::move, ::load, ::finish, and ::recoverActiveRooms. Store exactly:
  - gomoku:room:<id>
  - gomoku:active-room-ids
  - gomoku:next-room-id

move holds the room command mutex, reloads the persisted snapshot, constructs GameRoom, applies a move, persists the replacement, then updates local cache. A failed write returns kUnavailable and does not alter local state. finish writes the terminal snapshot before removing the room ID from the active index.

- [ ] Verify GREEN: ctest --test-dir HTTPServer/build -R 'game_room_test|pvp_game_service_test' --output-on-failure.

### Task 4: Connect The Service To Real Raft

**Files:**
- Create: HTTPServer/WebApps/GomokuServer/src/RaftGameStateStore.cpp
- Create: HTTPServer/WebApps/GomokuServer/test/RaftGameStateStoreTest.cpp
- Modify: HTTPServer/WebApps/GomokuServer/include/GomokuServer.h
- Modify: HTTPServer/WebApps/GomokuServer/src/GomokuServer.cpp
- Modify: HTTPServer/WebApps/GomokuServer/src/main.cpp
- Modify: HTTPServer/CMakeLists.txt

- [ ] Write RED adapter test using FakeClerk:

~~~
FakeClerk clerk;
clerk.nextPutStatus = ClerkStatus::kUnavailable;
RaftGameStateStore store(clerk, std::chrono::milliseconds(500));
assert(!store.store("gomoku:room:1", Json::object()));
assert(clerk.lastDeadline == std::chrono::milliseconds(500));
~~~

- [ ] Run RED: ctest --test-dir HTTPServer/build -R raft_game_state_store_test --output-on-failure. Expected: target is absent.

- [ ] Implement RaftGameStateStore with a 500 ms deadline. Parse -r <raft-config> in main.cpp. During GomokuServer initialization create one Clerk, the store, and PvpGameService, then call recoverActiveRooms before registering routes. Link simple_server against the prebuilt raft_kv_client library. Build raft_KV before HTTPServer; retain both CMake projects.

- [ ] Verify GREEN: cmake --build raft_KV/build --target raft_kv_client && cmake -S HTTPServer -B HTTPServer/build && cmake --build HTTPServer/build --target raft_game_state_store_test simple_server.

### Task 5: Commit WebSocket Commands Before Broadcast

**Files:**
- Modify: HTTPServer/WebApps/GomokuServer/include/handlers/GameWsHandler.h
- Modify: HTTPServer/WebApps/GomokuServer/src/handlers/GameWsHandler.cpp
- Modify: HTTPServer/WebApps/GomokuServer/include/GomokuServer.h
- Modify: HTTPServer/WebApps/GomokuServer/src/GomokuServer.cpp

- [ ] Write RED handler test:

~~~
auto response = harness.sendMove(roomId, blackPlayerId, 4, 4);
assert(response.type == "raft_unavailable");
assert(!harness.wasBroadcast());
assert(harness.persistedRoom(roomId).at("board").at(4).at(4) == "empty");
~~~

- [ ] Run RED: ctest --test-dir HTTPServer/build -R game_ws_handler_test --output-on-failure. Expected: target is absent.

- [ ] Make match creation persist before match_found. Replace direct GameRoom mutation in handleMove with PvpGameService::move. On failure send only:
  { "type": "raft_unavailable", "roomId": roomId }
On success send move_result with a complete state field to both players. Add state_request that returns:
  { "type": "state_result", "state": <snapshot> }
On finish, logout, leave, and disconnect, call finish before local cleanup.

- [ ] Verify GREEN: ctest --test-dir HTTPServer/build --output-on-failure.

### Task 6: Fix Client Rendering And Sync Recovery

**Files:**
- Create: HTTPServer/WebApps/GomokuServer/resource/pvp-client.js
- Modify: HTTPServer/WebApps/GomokuServer/resource/ChessGameVsPlayer.html
- Create: HTTPServer/WebApps/GomokuServer/test/pvp_frontend_smoke_test.js

- [ ] Write RED browser-state test:

~~~
applySnapshot({ board: boardWithBlackAt(2, 3), currentTurn: 22, gameOver: false });
assert.equal(cellAt(2, 3).classList.contains("black-stone"), true);
assert.equal(myTurn, false);
~~~

- [ ] Run RED: node HTTPServer/WebApps/GomokuServer/test/pvp_frontend_smoke_test.js. Expected: no snapshot renderer exists.

- [ ] Extract the inline client into pvp-client.js. applySnapshot must toggle black-stone and white-stone from the full board, update gameOver, derive myTurn from currentTurn, and update the indicator. Use it for move_result and state_result. For raft_unavailable show a system message, set myTurn false, and send state_request. Do not render a clicked move optimistically.

- [ ] Verify GREEN: node HTTPServer/WebApps/GomokuServer/test/pvp_frontend_smoke_test.js.

### Task 7: Add Build, Demo, And Monorepo Scripts

**Files:**
- Create: scripts/build.sh
- Create: scripts/start-demo.sh
- Create: scripts/stop-demo.sh
- Create: scripts/verify-monorepo.sh
- Create: README.md

- [ ] Write RED: run ./scripts/verify-monorepo.sh. Expected: script absent.

- [ ] build.sh configures and builds raft_KV/build before HTTPServer/build. start-demo.sh creates runtime/raft-nodes.conf, backgrounds raftCoreRun -n 3 -f runtime/raft-nodes.conf, waits for that configuration, then starts simple_server -p 8080 -r runtime/raft-nodes.conf. stop-demo.sh reads PID files and sends TERM. verify-monorepo.sh exports both nested repositories with git archive to mktemp, initializes one repository, and fails on mode 160000.

- [ ] README.md documents prerequisites, MySQL setup, build/start commands, leader-failover demonstration, PVP-only scope, one-writer limitation, and the final fresh-root Git procedure after nested .git directories are removed.

- [ ] Verify: bash -n scripts/*.sh && ./scripts/verify-monorepo.sh && ./scripts/build.sh.

### Task 8: Evidence

- [ ] Run:
  ctest --test-dir HTTPServer/build --output-on-failure
  ctest --test-dir raft_KV/build --output-on-failure
  node HTTPServer/WebApps/GomokuServer/test/pvp_frontend_smoke_test.js

Expected: every command exits 0.

- [ ] Run ./scripts/start-demo.sh. Match two authenticated sessions, make a move, terminate the Raft leader identified in runtime/raft.log, wait for a new leader, make another move, restart only simple_server, and confirm board plus turn restore from the committed snapshot.

Expected: no uncommitted move is broadcast during quorum loss; recovery works when a majority is available.

## Plan Review

- All accepted requirements map to Tasks 1-8.
- PVP persistence has exactly one path: GameRoom snapshot, GameStateStore, then PvpGameService.
- This plan intentionally excludes multi-application writes, Raft-executed game rules, history, replay, and AI persistence.

