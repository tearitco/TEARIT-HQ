#!/bin/sh
# build_spawn_lint.sh - build ops/+x/spawn_lint.+x (the unregistered-launch ratchet). 2026-11-02.
set -e
cd "$(dirname "$0")"
mkdir -p +x
echo "-- spawn_lint -> +x/spawn_lint.+x"
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/spawn_lint.+x spawn_lint.c
echo "OK +x/spawn_lint.+x"
