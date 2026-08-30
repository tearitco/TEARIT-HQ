#!/bin/bash
# launch_hq_info_window.sh - spawn a khtpm-style info window showing
# piececraft-hq's live game state, positioned below the real game window.
#
# Phase 1 v1 note: this is a simple bash launcher that shows the state
# file in a basic text window. Full integration into khtpm_entity_menu_
# render.c (with real styling, nav, interactions) is future work.
#
# Usage: bash launch_hq_info_window.sh <session_dir>

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <session_dir>"
    exit 1
fi

SESSION_DIR="$1"
STATE_FILE="$SESSION_DIR/piececraft-hq_state.txt"

# Real window positioning via xwininfo/XGetGeometry pattern
# Find the game window (board-viewer or gl_mirror will have created one)
find_game_window() {
    # Look for windows with "piececraft-hq" or "RGB mirror" in the title
    # (board-viewer/gl_mirror follow a consistent naming pattern)
    xwininfo -root -tree 2>/dev/null | grep -E "(piececraft-hq|RGB mirror)" | head -1 | awk '{print $1}'
}

# Wait up to 10 seconds for game window to appear
GAME_WIN=""
for i in {1..20}; do
    GAME_WIN=$(find_game_window)
    if [ -n "$GAME_WIN" ]; then
        break
    fi
    sleep 0.5
done

if [ -z "$GAME_WIN" ]; then
    echo "Error: game window not found" >&2
    exit 1
fi

# Get game window geometry (x, y, width, height)
GEOM=$(xwininfo -id "$GAME_WIN" 2>/dev/null | grep -E "Absolute upper-left|Width|Height" | awk '{print $NF}')
GAME_X=$(echo "$GEOM" | sed -n '1p')
GAME_Y=$(echo "$GEOM" | sed -n '2p')
GAME_W=$(echo "$GEOM" | sed -n '3p')
GAME_H=$(echo "$GEOM" | sed -n '4p')

# Position info window just below the game window
INFO_Y=$((GAME_Y + GAME_H + 10))
INFO_X=$GAME_X
INFO_W=400
INFO_H=300

# Launch a simple tail viewer in an xterm window
# This is v1 - real khtpm rendering integration is future work
if command -v xterm >/dev/null 2>&1; then
    xterm -geometry ${INFO_W}x${INFO_H}+${INFO_X}+${INFO_Y} \
          -title "piececraft-hq Info" \
          -e bash -c "while true; do clear; echo '=== Piececraft-HQ Live Status ==='; cat '$STATE_FILE' 2>/dev/null || echo 'Waiting for state...'; echo ''; echo '(Press Ctrl+C to close)'; sleep 1; done" &
else
    # Fallback: simple tail to console if xterm not available
    echo "xterm not available - displaying state on console only"
    tail -f "$STATE_FILE" 2>/dev/null
fi
