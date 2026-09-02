#!/bin/bash
# Open stats for current or specified hai session
# Usage: open_session_stats.sh [session_id] [house_root]

SESSION_ID="${1:-}"
HOUSE_ROOT="${2:-.}"

if [ -z "$SESSION_ID" ]; then
    # Use latest session if not specified
    SESSION_ID=$(ls -1 "$(cd "$HOUSE_ROOT" && pwd)"/&.widgits/open-hai/sessions 2>/dev/null | sort -rn | head -1)
fi

if [ -z "$SESSION_ID" ]; then
    echo "open_session_stats: no session found" >&2
    exit 1
fi

exec bash "$HOUSE_ROOT/&.hq-apps/stats-hq/open_stats_hq.sh" "$HOUSE_ROOT" "$SESSION_ID"
