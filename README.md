# Raft Gomoku Demonstration

This repository combines a Muduo HTTP/WebSocket Gomoku application with a Raft-backed key-value cluster. The first
release persists only PVP game state. Accounts, sessions, matchmaking, chat, and AI games remain in the HTTP
application and are outside the Raft state model.

## Prerequisites

- CMake 3.22 or newer and a C++20 compiler
- Protobuf, Muduo, Boost serialization, OpenSSL, pthread, and dl
- MySQL plus MySQL Connector/C++ with the legacy `cppconn` headers
- Node.js for the browser smoke test

The demo starts the HTTP application on port `8080` and lets the Raft launcher select three consecutive local ports.

## Build And Run

From the repository root:

```bash
mysql -u <admin-user> -p < scripts/init-db.sql
export GOMOKU_DB_HOST=tcp://127.0.0.1:3306
export GOMOKU_DB_USER=<gomoku-user>
export GOMOKU_DB_PASSWORD=<gomoku-password>
export GOMOKU_DB_NAME=Gomoku
./scripts/build.sh
./scripts/start-demo.sh
```

Create the MySQL user and grant it access to `Gomoku` before setting the variables. The application does not supply
database credentials or defaults; it exits with the missing variable's name when this setup is incomplete.

The runtime configuration, logs, and PID files are written beneath `runtime/`. Stop both the application and the Raft
process group with:

```bash
./scripts/stop-demo.sh
```

## Demonstration Procedure

1. Open two authenticated browser sessions at `http://127.0.0.1:8080`, enter PVP matching, and make a move.
   The move is sent to both players only after the Raft write returns successfully.
2. Identify the elected leader index from the latest `elect success` line in `runtime/raft.log`. The corresponding
   line number (starting at one for `raft{0}`) in `runtime/raft-node-pids` is its process ID; send that process
   `TERM`. Wait for a new Leader and make another move; the committed state continues when a majority remains
   available.
3. Stop only the HTTP application process group, then run `./scripts/start-demo.sh`; the latter reuses the running
   Raft cluster:

   ```bash
   /bin/kill -TERM -- "-$(<runtime/http.pid)"
   ./scripts/start-demo.sh
   ```

   Reconnect both players and request the room state. The active board and current turn are recovered from the
   committed Raft snapshot.
4. Stop the complete demo with `./scripts/stop-demo.sh`.

During a quorum loss, a PVP move returns `raft_unavailable`. The browser reloads the latest committed state and does
not replay the click automatically.

## Operational Limits

One HTTP application process is the PVP writer. Its room-level critical section covers read, validation, Raft write,
and local cache update. Multiple application writers are not supported. Raft stores an active room's complete
snapshot; finished rooms remain stored but are removed from the active-room index.

## Repository Check

Run the following after any repository consolidation work:

```bash
./scripts/verify-monorepo.sh
```

It exports `HTTPServer` and `raft_KV` from the current commit into a temporary repository and fails if either becomes
a Git submodule (mode `160000`).

## Fresh Root Consolidation

For the final fresh root repository, work in a copy of the project, remove only the copied nested `.git` directories,
then initialize and commit the copy as one repository. Do not remove metadata from an original checkout.

```bash
rm -rf <fresh-root>/HTTPServer/.git <fresh-root>/raft_KV/.git
git -C <fresh-root> init
git -C <fresh-root> add HTTPServer raft_KV
git -C <fresh-root> commit -m "初始化 Raft 五子棋单仓库"
```

Run `./scripts/verify-monorepo.sh` from that fresh root before publishing it.
