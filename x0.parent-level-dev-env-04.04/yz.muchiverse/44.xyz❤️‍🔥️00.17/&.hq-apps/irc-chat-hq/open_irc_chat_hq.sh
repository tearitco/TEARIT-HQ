#!/bin/bash
# open_irc_chat_hq.sh — launch the IRC Chat HQ window shell (NETWORK-
# CELL-HQ-WINDOWS-DESIGN.md §12), real HQ-style window replacing
# opencode's wrong-direction gnome-terminal wrapper. Same real launch
# shape as open_db_hq.sh: reuses the SAME shared khtpm_entity_menu_
# render.+x binary (no new C, class="irc-chat-window" falls through to
# the generic renderer), single-instance kill-before-relaunch, PID
# recorded for the taskbar's own kill-switch.
# Usage: open_irc_chat_hq.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open_irc_chat_hq: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$OPS_DIR/+x/khtpm_core_render.+x"
CHTPM="$HOUSE_ROOT/&.hq-apps/irc-chat-hq/irc-chat-hq.chtpm"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_core_render.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "open_irc_chat_hq: build failed, missing $BIN" >&2
    exit 1
fi

# Real, deliberately specific match (same real reason open_db_hq.sh's
# own db_hq_pids() is chtpm-path-specific, not a bare binary-name
# match) - khtpm_core_render.+x is a real, genuinely shared
# binary across many HQ windows now.
irc_hq_pids() { pgrep -f "khtpm_core_render\.\+x .*irc-chat-hq\.chtpm" 2>/dev/null || true; }

pids="$(irc_hq_pids)"
if [ -n "$pids" ]; then
    echo "open_irc_chat_hq: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(irc_hq_pids)"
    if [ -n "$pids" ]; then
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM" \
    >/tmp/irc-chat-hq.log 2>&1 < /dev/null &
echo $! >> "$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

if [ -n "$(irc_hq_pids)" ]; then
    echo "irc-chat-hq launched (logs=/tmp/irc-chat-hq.log)"
else
    echo "open_irc_chat_hq: launch failed, no live process found" >&2
    cat /tmp/irc-chat-hq.log 2>/dev/null >&2
    exit 1
fi
