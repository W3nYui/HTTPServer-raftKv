#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

for script in build.sh generate-dev-cert.sh start-demo.sh stop-demo.sh verify-monorepo.sh; do
  test -x "scripts/$script"
  bash -n "scripts/$script"
done

grep -q 'cmake -S .*raft_KV' scripts/build.sh
grep -q 'cmake -S .*HTTPServer' scripts/build.sh
grep -q 'raftCoreRun' scripts/start-demo.sh
grep -q -- '-n 3 -f' scripts/start-demo.sh
grep -q 'simple_server' scripts/start-demo.sh
grep -q -- '-P "$HTTPS_PORT"' scripts/start-demo.sh
grep -q -- '-c "$TLS_CERTIFICATE"' scripts/start-demo.sh
grep -q -- '-k "$TLS_PRIVATE_KEY"' scripts/start-demo.sh
grep -q 'setsid' scripts/start-demo.sh
grep -q 'kill -TERM' scripts/stop-demo.sh
grep -q ' archive --format=tar ' scripts/verify-monorepo.sh
grep -q '160000' scripts/verify-monorepo.sh

grep -q 'HTTPS' README.md
grep -q 'WSS' README.md
grep -q 'PVP' README.md
grep -q 'Leader' README.md
grep -q 'generate-dev-cert.sh' README.md
grep -q 'databaseConfigFromEnvironment' HTTPServer/WebApps/GomokuServer/test/test_database_config.cpp
grep -q 'CREATE TABLE IF NOT EXISTS users' scripts/init-db.sql
grep -q 'runtime/http.pid' README.md
grep -q 'docs/data-flow.md' README.md
grep -q 'WebSocket 输入帧' docs/data-flow.md

./scripts/verify-monorepo.sh

readonly TEST_RUNTIME_DIR="$(mktemp -d)"
trap 'rm -rf "$TEST_RUNTIME_DIR"' EXIT

CERT_DIR="$TEST_RUNTIME_DIR/certs" ./scripts/generate-dev-cert.sh
openssl verify -CAfile "$TEST_RUNTIME_DIR/certs/local-ca.crt" "$TEST_RUNTIME_DIR/certs/server.crt"
openssl x509 -in "$TEST_RUNTIME_DIR/certs/server.crt" -noout -text | grep -q 'DNS:localhost, IP Address:127.0.0.1'

RAFT_RUNNER="$ROOT_DIR/scripts/test-fixtures/fake-raft-runner.sh" \
HTTP_SERVER="$ROOT_DIR/scripts/test-fixtures/fake-http-server.sh" \
HTTP_BUILD_DIR="$ROOT_DIR" \
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
