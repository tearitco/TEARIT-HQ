#!/bin/sh
# TPMOS pieces/display/renderer.c pattern for this window's text frame.
# state/ui.txt is the live snapshot (rewritten every projector pass).
# debug/frames/session_frame_history.txt is the review log: wiped once
# when this watcher starts, then appended only when ui.txt changes.
# The per-pid key mailbox (entity_menu_history/<pid>.txt) stays a
# separate file and is still truncated by the renderer.
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
UI="$HERE/state/ui.txt"
DEST="$HERE/debug/frames/session_frame_history.txt"
mkdir -p "$HERE/debug/frames"
printf '=== NEW SESSION at %s ===\n' "$(date '+%Y-%m-%d %H:%M:%S')" > "$DEST"
last=""
while true; do
    if [ -f "$UI" ]; then
        sum=$(md5sum "$UI" | awk '{print $1}')
        if [ "$sum" != "$last" ]; then
            last=$sum
            {
                printf '\n--- FRAME UPDATE at %s ---\n' "$(date '+%Y-%m-%d %H:%M:%S')"
                cat "$UI"
            } >> "$DEST"
        fi
    fi
    sleep 0.4
done
