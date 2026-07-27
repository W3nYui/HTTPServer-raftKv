#!/usr/bin/env bash

set -euo pipefail

config_file=""
while getopts 'n:f:' option; do
  case "$option" in
    f) config_file="$OPTARG" ;;
  esac
done

printf 'node0ip=127.0.0.1\nnode1ip=127.0.0.1\nnode2ip=127.0.0.1\n' >"$config_file"

run_node() {
  trap 'exit 0' TERM INT
  while true; do sleep 1; done
}

for _ in 0 1 2; do
  run_node &
done

trap 'exit 0' TERM INT
wait
