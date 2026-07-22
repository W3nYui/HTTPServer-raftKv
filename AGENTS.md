# Repository Guidelines

## Project Structure & Module Organization

This workspace combines two C++ projects:

- `HTTPServer/` contains the Muduo-based HTTP/WebSocket framework and the Gomoku application. Framework code is in `HttpServer/`; application handlers, game models, and HTML resources are in `WebApps/GomokuServer/`.
- `raft_KV/` contains the Raft-backed key-value store. `src/raftCore/` owns Raft and the KV state machine, `src/raftClerk/` owns the client, and `src/rpc/` owns the RPC layer. Runnable examples are under `example/`; focused checks are under `test/`.
- Root `docs/` records the integration design, ADRs, and implementation plan. Read `CONTEXT.md` before changing PVP/Raft terminology. `raft_KV/AGENTS.md` adds module-specific rules.

## Build, Test, and Development Commands

Build projects separately until the integration scripts exist:

```bash
cmake -S raft_KV -B raft_KV/build-codex
cmake --build raft_KV/build-codex -j2

cmake -S HTTPServer -B HTTPServer/build
cmake --build HTTPServer/build -j2
```

`HTTPServer` requires OpenSSL, Muduo, MySQL Connector/C++, and `nlohmann-json`. `raft_KV` requires CMake 3.22+, C++20, Protobuf, Muduo, Boost serialization, pthread, and dl. The existing Raft `build/` directory may contain a cache from another path; use a fresh build directory rather than editing it. Run `cmake --build raft_KV/build-codex --target format` to apply the Raft formatter.

## Coding Style & Naming Conventions

Preserve local style: HTTP code commonly uses four-space indentation; Raft follows its Google-derived `.clang-format` with two spaces and a 120-column limit. Use lowercase camel case for C++ implementation files, matching headers in `include/`. Treat `.proto` files as source of truth; never hand-edit generated `*.pb.h` or `*.pb.cc` files.

## Testing Guidelines

There is currently no shared test framework or coverage target. Add focused executable tests beside the affected module, named `test_<behavior>.cpp`, and register them with CTest when modifying CMake. Distributed changes must cover leader change, retry/deadline behavior, restart recovery, and snapshot handling. Run the smallest relevant target before a full build.

## Commit & Pull Request Guidelines

Existing histories use short, scoped Chinese summaries, for example `实现 websocket 升级` or `完善 raft 间通信`. Keep commits imperative and limited to one concern. In reviews, describe the affected HTTP/WebSocket or Raft/RPC path, list commands actually run, link the issue or plan task, and include logs or screenshots only when they demonstrate observable behavior.

## Security & Configuration

Do not commit database passwords, TLS keys, generated binaries, `build/`, `bin/`, or `lib/` artifacts. Replace hard-coded local paths and credentials with documented configuration before sharing the integrated project.

## Agent skills

### Issue tracker

工作事项使用 GitHub Issues；外部 PR 不作为分流来源。See `docs/agents/issue-tracker.md`.

### Triage labels

使用默认的五个分流标签：`needs-triage`、`needs-info`、`ready-for-agent`、`ready-for-human`、`wontfix`。See `docs/agents/triage-labels.md`.

### Domain docs

单上下文布局；Raft 五子棋集成任务还应参考 `docs/superpowers/`。See `docs/agents/domain.md`.
