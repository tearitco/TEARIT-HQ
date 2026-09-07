#!/bin/sh
# rezip-house.sh — timestamped .7z backup of the house source tree.
# Excludes:
#   - .git (GitHub is the history backup)
#   - disposable generated data (framebuffers, ascii-frame mirror, logs) —
#     same policy as
#     x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.desktop/tidy-runtime.sh
#   - LIVE append-only files that running house processes (pals, taskbar,
#     ledgers, relays) write continuously — archiving one mid-write gives
#     a torn copy and makes 7z exit with a warning. These are pure
#     runtime state, never source.
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
    -xr'!node_modules' \
    -xr'!history.txt' \
    -xr'!entity_menu_history' -xr'!entity_menu_frame_*.txt' \
    -xr'!strip_history.txt' -xr'!strip_ascii_pulse.txt' -xr'!strip_frame_changed.txt' \
    -xr'!*.pulse.txt' -xr'!renderer_pulse.txt' -xr'!*_changed.txt' \
    -xr'!*_ledger.txt' -xr'!pending_tx.txt' \
    -xr'!*.seq' \
    -xr'!*.frame.txt'
echo "OK $OUT"
