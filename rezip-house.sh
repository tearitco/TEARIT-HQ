#!/bin/sh
# rezip-house.sh — timestamped .7z backup of the house source tree.
# Excludes .git and the disposable generated categories (framebuffers,
# ascii-frame mirror, logs) — same policy as
# x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.desktop/tidy-runtime.sh
# Deletes the previous house .7z so only the newest is kept.
set -e
cd "$(dirname "$0")"
SRC="x0.parent-level-dev-env-04.04"
OUT="${SRC}_$(date +%Y%m%d-%H%M%S).7z"

rm -f ${SRC}_*.7z

7z a -mx=3 "$OUT" "$SRC" \
    -xr'!.git' \
    -xr'!*.raw' -xr'!*.rgba32' \
    -xr'!ascii_frames' \
    -xr'!*.log' -xr'!*frame_history.txt' -xr'!gl_cli_out.txt' \
    -xr'!node_modules'
echo "OK $OUT"
