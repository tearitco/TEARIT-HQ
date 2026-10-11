#!/bin/sh
# build_game_snapshot_op.sh - builds game_snapshot_op into +x/ (content-addressed snapshot store + restore; verify: pal harness harness/game_snapshot.pal).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -Wno-misleading-indentation -O2 -o +x/game_snapshot_op.+x game_snapshot_op.c
echo "OK +x/game_snapshot_op.+x"
