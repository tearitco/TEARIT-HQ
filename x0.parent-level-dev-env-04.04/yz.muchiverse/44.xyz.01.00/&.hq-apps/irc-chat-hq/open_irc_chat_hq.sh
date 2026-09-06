#!/bin/sh
# open_irc_chat_hq.sh - launch the irc-chat-hq X11-HQ window.
# Shape = &.hq-apps/chat-hai/button-pal.sh: this script launches ONE
# process (the shared renderer on irc-chat-hq.xhtpm); the template's
# single <module> (ops/+x/irc_chat_manager.+x) is forked by the
# renderer's own kh_launch_window_modules() and SIGTERM'd on close.
#
#   sh open_irc_chat_hq.sh <house_root>
#
# Wired into the taskbar network cell: livedesk:open-network:irc points
# here (retargeted from the CLI 044.pal-chat-irc👥️+2/button.sh).
set -e

HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open_irc_chat_hq.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/irc-chat-hq.xhtpm"
MGR="$HERE/ops/+x/irc_chat_manager.+x"
RENDER_OPS_DIR="$HOUSE_ROOT"/*.monads/*.livedesk-taskbar/ops
BIN="$(cd $RENDER_OPS_DIR && pwd)/+x/khtpm_core_render.+x"

[ -x "$BIN" ] || (cd $RENDER_OPS_DIR && sh build_core_render.sh) || true
[ -x "$MGR" ] || (cd "$HERE/ops" && sh build_irc_chat_manager.sh) || true
for f in "$BIN" "$MGR" "$XHTPM"; do
    [ -e "$f" ] || { echo "open_irc_chat_hq.sh: missing $f" >&2; exit 1; }
done

LOG_DIR="$HERE/audit"
mkdir -p "$LOG_DIR"

mine() { pgrep -f "khtpm_core_render\.\+x.*irc-chat-hq\.xhtpm" 2>/dev/null || true; }
strays() { pgrep -f "irc-chat-hq/ops/\+x/irc_chat_manager\.\+x" 2>/dev/null || true; }

pids="$(mine; strays)"
pids="$(echo "$pids" | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "open_irc_chat_hq.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(mine; strays)"
    pids="$(echo "$pids" | grep -v '^$' || true)"
    [ -n "$pids" ] && { echo "$pids" | xargs -r kill -KILL; sleep 1; }
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" \
    >"$LOG_DIR/irc-chat-hq.log" 2>&1 < /dev/null &
echo $! >> "$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

n="$(mine | grep -c . || true)"
if [ "$n" -ge 1 ] 2>/dev/null; then
    echo "irc-chat-hq launched (renderer pid $(mine | tr '\n' ' '), log=$LOG_DIR/irc-chat-hq.log)"
else
    echo "open_irc_chat_hq.sh: FAILED to launch - check the log:" >&2
    cat "$LOG_DIR/irc-chat-hq.log" 2>/dev/null >&2
    exit 1
fi
