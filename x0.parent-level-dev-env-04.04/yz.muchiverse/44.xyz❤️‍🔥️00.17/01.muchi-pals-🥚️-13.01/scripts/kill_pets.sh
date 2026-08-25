#!/bin/sh
# kill_pets.sh - emergency cleanup: force-closes every floating pet
# window (system/egg_window[.exe]), regardless of whether its terminal
# session is still running, whether it has a working session-PID check,
# or whether its own right-click "Close" context menu is reachable.
#
# Why this exists: menu_input.c's spawn_egg_window now tracks one
# window.pid marker per pet and passes the session PID through so a
# window closes itself when its session ends or a duplicate open is
# requested - but that's a *normal-path* safeguard, not an escape hatch.
# This script is the escape hatch: run it any time regardless of what
# state the rest of the game is in, no menu navigation required.
#
# Usage: sh scripts/kill_pets.sh
set -e
cd "$(dirname "$0")/.."

echo "=== Killing all egg-pals pet windows ==="

if command -v pkill >/dev/null 2>&1; then
    pkill -f "system/egg_window" 2>/dev/null
else
    # pkill isn't installed by default under MSYS2/Git-for-Windows -
    # fall back to matching `ps` output and killing by PID directly,
    # same approach button.sh's own kill_matching() uses.
    ps 2>/dev/null | grep -F "system/egg_window" | grep -v grep | awk '{print $1}' | while read -r pid; do
        kill "$pid" 2>/dev/null
    done
fi

# Stale window.pid markers left behind don't hurt anything (the next
# Open Window attempt checks liveness and overwrites them), but clearing
# them now makes the immediate post-cleanup state easier to reason about.
find pieces/world_01/map_lobby -maxdepth 2 -name "window.pid" -exec rm -f {} + 2>/dev/null

echo "done"
