#!/bin/sh
# build_robot_chat_manager.sh - compiles khtpm_robot_chat_manager.+x,
# same real per-binary hash-gate convention run_khtpm_strip.sh/
# build_khtpm_strip.sh already use (EVENT-MODULARITY-AND-BUILD-SPEED.md
# §2). Standalone (not folded into build_khtpm_strip.sh) matching
# open-hai/events-hq's own each-widget-builds-itself convention.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
SHARED_DIR="$(cd "$HERE/../../_shared-lib" 2>/dev/null && pwd || echo /nonexistent)"
MANIFEST="$HERE/.build_hashes.pdl"
. "$SHARED_DIR/hash_gate.sh"

mkdir -p "$HERE/+x"
if hash_gate_stale "$MANIFEST" "$HERE/+x/khtpm_robot_chat_manager.+x" "$HERE/khtpm_robot_chat_manager.c"; then
    gcc -O2 -Wall -o "$HERE/+x/khtpm_robot_chat_manager.+x" "$HERE/khtpm_robot_chat_manager.c"
    hash_gate_commit "$MANIFEST" "$HERE/+x/khtpm_robot_chat_manager.+x" "$HERE/khtpm_robot_chat_manager.c"
    echo "-- built khtpm_robot_chat_manager.+x"
else
    echo "-- khtpm_robot_chat_manager.+x up to date (hash unchanged), skipping compile"
fi
