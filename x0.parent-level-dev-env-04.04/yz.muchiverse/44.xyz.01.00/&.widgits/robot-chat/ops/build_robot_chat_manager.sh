#!/bin/sh
# build_robot_chat_manager.sh - compiles the two real ops
# robot_chat.pal's own poll loop calls: rc_check_request.+x and
# rc_publish_ui.+x. Same per-binary hash-gate convention every other
# build_*.sh already uses (EVENT-MODULARITY-AND-BUILD-SPEED.md §2).
#
# REAL REWORK 2026-09-28 (direct instruction: "the manager should be a
# pal script instead of C") - this file used to build a single
# hand-written C manager (khtpm_robot_chat_manager.c, now retired,
# never reached a working verified state). Kept the same filename so
# button.sh's own existing call site needs no change - it now builds
# ops instead, matching robot_chat.pal's real ../ops/+x/ layout.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
SHARED_DIR="$(cd "$HERE/../../_shared-lib" 2>/dev/null && pwd || echo /nonexistent)"
MANIFEST="$HERE/.build_hashes.pdl"
. "$SHARED_DIR/hash_gate.sh"

mkdir -p "$HERE/+x"
for name in rc_check_request rc_publish_ui; do
    if hash_gate_stale "$MANIFEST" "$HERE/+x/$name.+x" "$HERE/$name.c"; then
        gcc -O2 -Wall -o "$HERE/+x/$name.+x" "$HERE/$name.c"
        hash_gate_commit "$MANIFEST" "$HERE/+x/$name.+x" "$HERE/$name.c"
        echo "-- built $name.+x"
    else
        echo "-- $name.+x up to date (hash unchanged), skipping compile"
    fi
done
