#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

for script in build.sh start-demo.sh stop-demo.sh verify-monorepo.sh; do
  test -x "scripts/$script"
  bash -n "scripts/$script"
done

grep -q 'cmake -S .*raft_KV' scripts/build.sh
grep -q 'cmake -S .*HTTPServer' scripts/build.sh
grep -q 'raftCoreRun' scripts/start-demo.sh
grep -q -- '-n 3 -f' scripts/start-demo.sh
grep -q 'simple_server' scripts/start-demo.sh
grep -q 'setsid' scripts/start-demo.sh
grep -q 'kill -TERM' scripts/stop-demo.sh
grep -q ' archive --format=tar ' scripts/verify-monorepo.sh
grep -q '160000' scripts/verify-monorepo.sh

grep -q 'PVP' README.md
grep -q 'One HTTP application process' README.md
grep -q 'Leader' README.md
grep -q 'databaseConfigFromEnvironment' HTTPServer/WebApps/GomokuServer/test/test_database_config.cpp
grep -q 'CREATE TABLE IF NOT EXISTS users' scripts/init-db.sql
grep -q 'runtime/http.pid' README.md
grep -q 'fresh root' README.md

./scripts/verify-monorepo.sh

readonly TEST_RUNTIME_DIR="$(mktemp -d)"
trap 'rm -rf "$TEST_RUNTIME_DIR"' EXIT

RAFT_RUNNER="$ROOT_DIR/scripts/test-fixtures/fake-raft-runner.sh" \
HTTP_SERVER="$ROOT_DIR/scripts/test-fixtures/fake-http-server.sh" \
RUNTIME_DIR="$TEST_RUNTIME_DIR" \
./scripts/start-demo.sh

test -s "$TEST_RUNTIME_DIR/raft.pid"
test -s "$TEST_RUNTIME_DIR/http.pid"
test -f "$TEST_RUNTIME_DIR/raft-node-pids"
test "$(wc -l <"$TEST_RUNTIME_DIR/raft-node-pids")" -eq 3
raft_pid="$(<"$TEST_RUNTIME_DIR/raft.pid")"
http_pid="$(<"$TEST_RUNTIME_DIR/http.pid")"
kill -0 "$raft_pid"
kill -0 "$http_pid"

RUNTIME_DIR="$TEST_RUNTIME_DIR" ./scripts/stop-demo.sh
! kill -0 "$raft_pid" 2>/dev/null
! kill -0 "$http_pid" 2>/dev/null
