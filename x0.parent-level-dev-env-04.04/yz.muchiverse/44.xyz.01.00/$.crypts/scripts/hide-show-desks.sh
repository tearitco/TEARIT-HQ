#!/bin/bash
STATE_FILE="$HOME/.cache/muchiverse-desk-visibility"
HOUSE="$(cd "$(dirname "$0")/../.." && pwd)"
OPEN_FILE="$HOUSE/#.desktop/livedesk_open.txt"

mkdir -p "$HOME/.cache" 2>/dev/null

if [ -f "$STATE_FILE" ]; then
    # Hidden → Visible: unfreeze all
    while IFS= read -r line; do
        pid=$(echo "$line" | sed 's/^PID=\([0-9]*\).*/\1/')
        [ -n "$pid" ] && kill -CONT "$pid" 2>/dev/null
    done < <(grep "^PID=" "$OPEN_FILE" 2>/dev/null)
    rm -f "$STATE_FILE"
    echo "👁️  All desk-pals visible"
else
    # Visible → Hidden: freeze all
    while IFS= read -r line; do
        pid=$(echo "$line" | sed 's/^PID=\([0-9]*\).*/\1/')
        [ -n "$pid" ] && kill -STOP "$pid" 2>/dev/null
    done < <(grep "^PID=" "$OPEN_FILE" 2>/dev/null)
    echo "1" > "$STATE_FILE"
    echo "🙈 All desk-pals hidden"
fi
