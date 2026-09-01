#!/bin/bash
# open_forum_hq.sh — launch the Forum HQ window shell (NETWORK-CELL-HQ-
# WINDOWS-DESIGN.md §12). Same real shape as open_irc_chat_hq.sh - see
# that script's own header comment for the full reasoning.
# Usage: open_forum_hq.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open_forum_hq: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$OPS_DIR/+x/khtpm_core_render.+x"
CHTPM="$HOUSE_ROOT/&.hq-apps/forum-hq/forum-hq.chtpm"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_core_render.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "open_forum_hq: build failed, missing $BIN" >&2
    exit 1
fi

forum_hq_pids() { pgrep -f "khtpm_core_render\.\+x .*forum-hq\.chtpm" 2>/dev/null || true; }

pids="$(forum_hq_pids)"
if [ -n "$pids" ]; then
    echo "open_forum_hq: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(forum_hq_pids)"
    if [ -n "$pids" ]; then
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM" \
    >/tmp/forum-hq.log 2>&1 < /dev/null &
echo $! >> "$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

if [ -n "$(forum_hq_pids)" ]; then
    echo "forum-hq launched (logs=/tmp/forum-hq.log)"
else
    echo "open_forum_hq: launch failed, no live process found" >&2
    cat /tmp/forum-hq.log 2>/dev/null >&2
    exit 1
fi
