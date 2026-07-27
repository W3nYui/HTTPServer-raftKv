#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly BUILD_ID="$(printf '%s' "$ROOT_DIR" | sha256sum | cut -c1-12)"
readonly RAFT_BUILD_DIR="$ROOT_DIR/raft_KV/build-codex-$BUILD_ID"
readonly HTTP_BUILD_DIR="$ROOT_DIR/HTTPServer/build-codex-$BUILD_ID"

cmake -S "$ROOT_DIR/raft_KV" -B "$RAFT_BUILD_DIR"
cmake --build "$RAFT_BUILD_DIR" -j2

cmake -S "$ROOT_DIR/HTTPServer" -B "$HTTP_BUILD_DIR"
cmake --build "$HTTP_BUILD_DIR" -j2
