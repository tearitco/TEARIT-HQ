#!/bin/sh
# open_canvas_craft.sh - launch the Canvas-Craft X11-HQ window.
# Shape = &.hq-apps/irc-chat-hq/open_irc_chat_hq.sh: launches ONE
# process (the shared renderer on canvas-craft.xhtpm); the template's
# single <module> (ops/+x/canvascraft_manager.+x) is forked by the
# renderer and SIGTERM'd on close.
#
#   sh open_canvas_craft.sh <house_root>
#
# Will be wired into the taskbar Palettes menu ("Canvas-Craft" row) in a
# later phase; for now it's a standalone launcher.
set -e

HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open_canvas_craft.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/canvas-craft.xhtpm"
MGR="$HERE/ops/+x/canvascraft_manager.+x"
RENDER_OPS_DIR="$HOUSE_ROOT"/*.monads/*.livedesk-taskbar/ops
BIN="$(cd $RENDER_OPS_DIR && pwd)/+x/khtpm_core_render.+x"

[ -x "$BIN" ] || (cd $RENDER_OPS_DIR && sh build_core_render.sh) || true
[ -x "$MGR" ] || (cd "$HERE/ops" && sh build_canvascraft_manager.sh) || true
for f in "$BIN" "$MGR" "$XHTPM"; do
    [ -e "$f" ] || { echo "open_canvas_craft.sh: missing $f" >&2; exit 1; }
done

LOG_DIR="$HERE/audit"
mkdir -p "$LOG_DIR"

mine() { pgrep -f "khtpm_core_render\.\+x.*canvas-craft\.xhtpm" 2>/dev/null || true; }
strays() { pgrep -f "canvas-craft/ops/\+x/canvascraft_manager\.\+x" 2>/dev/null || true; }

pids="$(mine; strays)"
pids="$(echo "$pids" | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "open_canvas_craft.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(mine; strays)"
    pids="$(echo "$pids" | grep -v '^$' || true)"
    [ -n "$pids" ] && { echo "$pids" | xargs -r kill -KILL; sleep 1; }
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" \
    >"$LOG_DIR/canvas-craft.log" 2>&1 < /dev/null &
printf '%s %s 0 0 canvas-craft\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

n="$(mine | grep -c . || true)"
if [ "$n" -ge 1 ] 2>/dev/null; then
    echo "canvas-craft launched (renderer pid $(mine | tr '\n' ' '), log=$LOG_DIR/canvas-craft.log)"
else
    echo "open_canvas_craft.sh: FAILED to launch - check the log:" >&2
    cat "$LOG_DIR/canvas-craft.log" 2>/dev/null >&2
    exit 1
fi
