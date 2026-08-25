#!/bin/sh
# Windows: EMERGENCY_CLOSE.ps1 (this file is Linux pgrep/kill; it will not run on Win).
# Emergency close all desk-pals (entity windows)
# Used when normal quit doesn't work
# POSIX-compliant (works with sh and bash)
#
# STATUS UPDATE (2026-08-11): the normal quit path (khtpm's X.quit menu row
# -> ktb_quit_and_save() in *.monads/*.livedesk-taskbar/ops/khtpm_taskbar_
# manager.c) now does this same SIGTERM-then-SIGKILL sweep automatically on
# every quit - see that function's own header comment. This script should
# no longer be load-bearing for routine quits; keep it only as a manual
# last resort for when khtpm's own processes are killed/crashed directly
# (bypassing the quit menu entirely) and its own sweep never got to run.

HOUSE="$(cd "$(dirname "$0")" && pwd)"
OPEN_FILE="$HOUSE/#.desktop/livedesk_open.txt"
PIDFILE="/tmp/emergency_pids_$$.txt"

trap "rm -f '$PIDFILE'" EXIT

echo "⚠️  EMERGENCY_CLOSE: Closing all desk-pals..."

# Extract PIDs from livedesk_open.txt first
if [ -f "$OPEN_FILE" ] && [ -s "$OPEN_FILE" ]; then
    grep "^PID=" "$OPEN_FILE" 2>/dev/null | sed 's/^PID=\([0-9]*\).*/\1/' > "$PIDFILE"
fi

# Fallback: find tp_desktop_window and khtpm subwindow processes if file was empty
if [ ! -s "$PIDFILE" ]; then
    echo "livedesk_open.txt empty, searching for entity and subwindow processes..."
    (pgrep -f "tp_desktop_window"; pgrep -f "khtpm_open_hai_render"; pgrep -f "khtpm_hq_render"; pgrep -f "khtpm_entity_menu_render") > "$PIDFILE" 2>/dev/null
    sort -u "$PIDFILE" -o "$PIDFILE"  # Remove duplicates
fi

if [ ! -s "$PIDFILE" ]; then
    echo "No desk-pals found to close"
    exit 0
fi

pid_count=$(wc -l < "$PIDFILE")
pids=$(tr '\n' ' ' < "$PIDFILE")
echo "Found $pid_count desk-pals: $pids"

# Phase 1: Try SIGTERM (graceful)
echo "Phase 1: Sending SIGTERM (graceful close)..."
while read -r pid; do
    [ -n "$pid" ] && kill -TERM "$pid" 2>/dev/null && echo "  SIGTERM sent to $pid" || echo "  Failed to send SIGTERM to $pid"
done < "$PIDFILE"

# Give processes time to shut down
sleep 1

# Phase 2: Check if any are still alive, kill with SIGKILL
echo "Phase 2: Checking for survivors..."
living_file="/tmp/emergency_living_$$.txt"
while read -r pid; do
    if kill -0 "$pid" 2>/dev/null; then
        echo "$pid" >> "$living_file"
        echo "  $pid still running - sending SIGKILL"
        kill -KILL "$pid" 2>/dev/null
    else
        echo "  $pid closed cleanly"
    fi
done < "$PIDFILE"

# Final check
sleep 0.5
echo "Phase 3: Final verification..."
if [ -f "$living_file" ]; then
    while read -r pid; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "  ✗ $pid STILL alive (zombie?)"
        else
            echo "  ✓ $pid confirmed dead"
        fi
    done < "$living_file"
    rm -f "$living_file"
fi

# Clean up state files
rm -f "$HOME/.cache/muchiverse-desk-visibility" 2>/dev/null
echo "✓ Emergency close complete"
