#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly RUNTIME_DIR="${RUNTIME_DIR:-$ROOT_DIR/runtime}"

terminate_process_group() {
  local name="$1"
  local pid_file="$2"

  if [[ ! -s "$pid_file" ]]; then
    return
  fi

  local pid
  pid="$(<"$pid_file")"
  if kill -0 "$pid" 2>/dev/null; then
    /bin/kill -TERM -- "-$pid"
    for _ in $(seq 1 10); do
      if ! kill -0 "$pid" 2>/dev/null; then
        break
      fi
      sleep 1
    done
    if kill -0 "$pid" 2>/dev/null; then
      /bin/kill -KILL -- "-$pid"
    fi
    echo "Stopped $name (process group $pid)"
  fi
  : >"$pid_file"
}

terminate_process_group "HTTP application" "$RUNTIME_DIR/http.pid"
terminate_process_group "Raft cluster" "$RUNTIME_DIR/raft.pid"
