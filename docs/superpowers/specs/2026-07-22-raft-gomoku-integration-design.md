# Raft Gomoku Integration Design

## Goal

Deliver an interview-ready online Gomoku demonstration that combines the existing HTTP/WebSocket application with the existing Raft-backed KV store. A PVP move becomes visible to players only after a Raft quorum commits the resulting game state.

## Scope

- Integrate Raft only with PVP Gomoku.
- Keep user accounts in MySQL and keep existing sessions, chat, matchmaking, and AI games unchanged.
- Run one HTTP/WebSocket application process and a three-node Raft KV cluster.
- Recover active PVP games after the application process restarts.
- Retain the final snapshot of ended games without providing match history, replay, or search.

## Exclusions

- Multiple HTTP application instances and application-tier failover.
- Atomic `ApplyMove` commands inside the Raft state machine.
- Raft-backed AI games in the first release.
- Authentication, schema, or frontend redesign.

## Architecture

The browser continues to use the existing HTML, HTTP, and WebSocket endpoints. It never connects to Raft directly. `GomokuServer` owns authentication, connection management, match creation, and WebSocket broadcasts. A new application-side Raft gateway owns the existing `Clerk` client and presents bounded `load` and `store` operations to PVP code.

For a PVP move, the room-specific critical section performs this sequence:

1. Read the room snapshot from Raft.
2. Rebuild or refresh the local `GameRoom` from that snapshot.
3. Validate the player, turn, coordinates, game state, and occupancy.
4. Apply the move locally and serialize the complete resulting snapshot.
5. Write the snapshot through `Clerk::Put` with a bounded deadline.
6. Broadcast the new state only after that write reports success.

One application process is the single writer. Its room mutex spans this read-validate-write sequence, preventing two web-worker threads from writing incompatible snapshots. The design intentionally does not claim that two application processes can safely share the same room.

## Persistence Model

All values are JSON strings stored by the existing Raft KV API.

| Key | Value | Purpose |
| --- | --- | --- |
| `gomoku:room:<roomId>` | complete room snapshot | restores or displays one PVP game |
| `gomoku:active-room-ids` | JSON array of active room IDs | identifies recoverable games because the KV API cannot scan keys |
| `gomoku:next-room-id` | decimal integer | allocates unique room IDs across application restarts |

A room snapshot contains `roomId`, both player IDs, board cells, current turn, move count, terminal state, winner, terminal reason, and last move. It is replaced on every accepted move. When a game finishes, the room ID is removed from the active index but its room key remains as the final demonstration result.

## Failure Semantics

`Clerk` must stop retrying after a configured deadline and return an explicit failure to the gateway. If Raft has no reachable leader or no quorum, the application leaves its room unchanged, emits `raft_unavailable`, and broadcasts no move. The browser then requests the latest committed game state. It must not automatically replay the previous click because a timeout can occur after the write commits but before the client observes success.

## Browser Contract

Existing PVP WebSocket messages remain the transport. The integration adds a successful committed-state message or extends the existing move result with the complete committed snapshot. Failure messages use `raft_unavailable`; the browser disables the pending action, displays a synchronization error, and requests a fresh room state. Browser-side checks remain usability hints only. The server is the authority for move validity.

## Startup And Demonstration

The two CMake projects remain separate to avoid immediate dependency and language-standard consolidation. Root scripts build each project, start `raftCoreRun -n 3` to create the Raft cluster configuration, then start the Gomoku application with the generated configuration path. The demo sequence is: create a PVP match, make a committed move, terminate the current Raft leader, observe client retry and continued committed moves after election, restart the HTTP process, and confirm the active game is restored from Raft.

## Git Consolidation Validation

Before changing the working directory, create a disposable directory under `/tmp`, export the current `HTTPServer` and `raft_KV` tracked files into sibling directories without their nested `.git` directories, initialize one new Git repository, stage both directories as ordinary files, and make a single initial commit. The check passes only if `git ls-files --stage` contains regular files below both directories and no mode `160000` gitlinks. The original two repositories and their histories remain untouched during this validation.

## Acceptance Criteria

- A normal PVP move is persisted through Raft before it is broadcast to both players.
- An invalid or out-of-turn move does not alter the Raft snapshot.
- During Raft unavailability, a move produces `raft_unavailable`, changes neither local nor broadcast state, and the browser reloads committed state.
- After an HTTP application restart, each active room in `gomoku:active-room-ids` is restored with the same board and turn.
- An ended room is absent from the active index and retains its final snapshot.
- The temporary monorepo validation contains ordinary files from both projects, not submodules.
- The existing PVP regression is reproduced, root-caused, covered by a focused test, and fixed without regressing the integration tests.
