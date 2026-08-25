#!/bin/bash
# kill_avatar_windows.sh - FAST: only desktop avatar_window + window.pid
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
echo "=== kill avatar_window ==="

# Known PIDs first
if [ -d "$ROOT/pieces/world_01/map_lobby" ]; then
    for pf in "$ROOT/pieces/world_01/map_lobby"/*/window.pid; do
        [ -f "$pf" ] || continue
        pid=$(tr -d ' \n\r' < "$pf" 2>/dev/null || true)
        [ -n "$pid" ] && kill -TERM "$pid" 2>/dev/null || true
        rm -f "$pf"
    done
fi
if [ -f "$ROOT/pieces/system/avatar_window_pids.txt" ]; then
    while read -r pid; do
        [ -n "${pid:-}" ] && kill -TERM "$pid" 2>/dev/null || true
    done < "$ROOT/pieces/system/avatar_window_pids.txt" 2>/dev/null || true
    : > "$ROOT/pieces/system/avatar_window_pids.txt"
fi

# Exact name (fast; not chrome)
if pgrep -x avatar_window >/dev/null 2>&1; then
    pkill -x -TERM avatar_window 2>/dev/null || true
    sleep 0.08
    pkill -x -KILL avatar_window 2>/dev/null || true
fi

if pgrep -x avatar_window >/dev/null 2>&1; then
    echo "WARNING: avatar_window still running"
    exit 1
fi
echo "clean"
