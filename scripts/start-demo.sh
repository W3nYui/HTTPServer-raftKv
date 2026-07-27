#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly RUNTIME_DIR="${RUNTIME_DIR:-$ROOT_DIR/runtime}"
readonly BUILD_ID="$(printf '%s' "$ROOT_DIR" | sha256sum | cut -c1-12)"
readonly RAFT_RUNNER="${RAFT_RUNNER:-$ROOT_DIR/raft_KV/bin/raftCoreRun}"
readonly HTTP_BUILD_DIR="${HTTP_BUILD_DIR:-$ROOT_DIR/HTTPServer/build-codex-$BUILD_ID}"
readonly HTTP_SERVER="${HTTP_SERVER:-$HTTP_BUILD_DIR/simple_server}"
readonly HTTP_PORT="${HTTP_PORT:-8080}"
readonly HTTPS_PORT="${HTTPS_PORT:-8443}"
readonly RAFT_CONFIG="$RUNTIME_DIR/raft-nodes.conf"
readonly RAFT_PID_FILE="$RUNTIME_DIR/raft.pid"
readonly RAFT_NODE_PID_FILE="$RUNTIME_DIR/raft-node-pids"
readonly HTTP_PID_FILE="$RUNTIME_DIR/http.pid"
readonly TLS_CERT_DIR="${TLS_CERT_DIR:-$RUNTIME_DIR/certs}"
readonly TLS_CERTIFICATE="${TLS_CERTIFICATE:-$TLS_CERT_DIR/server.crt}"
readonly TLS_PRIVATE_KEY="${TLS_PRIVATE_KEY:-$TLS_CERT_DIR/server.key}"

mkdir -p "$RUNTIME_DIR"

if [[ -s "$HTTP_PID_FILE" ]] && kill -0 "$(<"$HTTP_PID_FILE")" 2>/dev/null; then
  echo "HTTP application is already running; stop it before starting another instance" >&2
  exit 1
fi

if [[ ! -x "$RAFT_RUNNER" || ! -x "$HTTP_SERVER" ]]; then
  echo "Build the demo first with scripts/build.sh" >&2
  exit 1
fi

if [[ ! -s "$TLS_CERTIFICATE" || ! -s "$TLS_PRIVATE_KEY" ]]; then
  CERT_DIR="$TLS_CERT_DIR" "$ROOT_DIR/scripts/generate-dev-cert.sh"
fi

if [[ -s "$RAFT_PID_FILE" ]] && kill -0 "$(<"$RAFT_PID_FILE")" 2>/dev/null; then
  raft_pid="$(<"$RAFT_PID_FILE")"
else
  setsid "$RAFT_RUNNER" -n 3 -f "$RAFT_CONFIG" >"$RUNTIME_DIR/raft.log" 2>&1 &
  raft_pid=$!
  printf '%s\n' "$raft_pid" >"$RAFT_PID_FILE"
fi

for _ in $(seq 1 20); do
  if [[ $(grep -Ec '^node[0-9]+ip=' "$RAFT_CONFIG" 2>/dev/null || true) -eq 3 ]]; then
    break
  fi
  if ! kill -0 "$raft_pid" 2>/dev/null; then
    echo "Raft process exited; see $RUNTIME_DIR/raft.log" >&2
    exit 1
  fi
  sleep 1
done

if [[ $(grep -Ec '^node[0-9]+ip=' "$RAFT_CONFIG" 2>/dev/null || true) -ne 3 ]]; then
  echo "Raft cluster did not publish three nodes; see $RUNTIME_DIR/raft.log" >&2
  exit 1
fi

ps -o pid= --ppid "$raft_pid" | sort -n >"$RAFT_NODE_PID_FILE"
if [[ $(wc -l <"$RAFT_NODE_PID_FILE") -ne 3 ]]; then
  echo "Raft launcher did not create three node processes; see $RUNTIME_DIR/raft.log" >&2
  exit 1
fi

while read -r node_pid; do
  if ! kill -0 "$node_pid" 2>/dev/null; then
    echo "Raft node process $node_pid exited during startup; see $RUNTIME_DIR/raft.log" >&2
    exit 1
  fi
done <"$RAFT_NODE_PID_FILE"

(
  cd "$HTTP_BUILD_DIR"
  exec setsid "$HTTP_SERVER" -p "$HTTP_PORT" -P "$HTTPS_PORT" -r "$RAFT_CONFIG" \
    -c "$TLS_CERTIFICATE" -k "$TLS_PRIVATE_KEY"
) >"$RUNTIME_DIR/http.log" 2>&1 &
http_pid=$!
printf '%s\n' "$http_pid" >"$HTTP_PID_FILE"

sleep 1
if ! kill -0 "$http_pid" 2>/dev/null; then
  echo "HTTP application exited; see $RUNTIME_DIR/http.log" >&2
  exit 1
fi

printf 'Demo started: http://127.0.0.1:%s redirects to https://127.0.0.1:%s\nRaft config: %s\n' \
  "$HTTP_PORT" "$HTTPS_PORT" "$RAFT_CONFIG"
