#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly CHECK_DIR="$(mktemp -d)"
trap 'rm -rf "$CHECK_DIR"' EXIT

git -C "$ROOT_DIR" archive --format=tar HEAD HTTPServer raft_KV | tar -xf - -C "$CHECK_DIR"
git -C "$CHECK_DIR" init -q
git -C "$CHECK_DIR" config user.name "monorepo verification"
git -C "$CHECK_DIR" config user.email "monorepo-verification@example.invalid"
git -C "$CHECK_DIR" add HTTPServer raft_KV
git -C "$CHECK_DIR" commit -qm "verify monorepo export"

if git -C "$CHECK_DIR" ls-files --stage | awk '$1 == "160000" { found = 1 } END { exit !found }'; then
  echo "Monorepo verification failed: exported projects contain gitlinks" >&2
  exit 1
fi

git -C "$CHECK_DIR" ls-files --error-unmatch HTTPServer/CMakeLists.txt raft_KV/CMakeLists.txt >/dev/null
echo "Monorepo verification passed: HTTPServer and raft_KV are regular tracked files."
