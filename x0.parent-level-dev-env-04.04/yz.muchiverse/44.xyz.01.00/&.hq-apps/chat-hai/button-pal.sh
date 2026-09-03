#!/bin/sh
# button-pal.sh - PARALLEL launcher for the static-xhtpm chat-hai window
# (HANDOFF-scope-nav-and-chtpm-port.md §5). Runs alongside the old
# button.sh + chat-hai.chtpm + ops/chat_hai_projector.sh (bash), which
# stay untouched as rollback. NOT wired into any menu.
#
#   sh button-pal.sh <house_root>
#
# Shape = &.widgits/open-hai/button.sh: this script launches ONE process
# (the shared renderer on chat-hai.xhtpm). The template's two <module>
# tags - ops/chat_hai_loop.sh (persona backend) and
# ops/+x/chat_hai_projector.+x (state -> state/ui.txt) - are forked by
# the renderer's own generic kh_launch_window_modules() and SIGTERM'd
# when the window closes.
set -e

HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "chat-hai button-pal.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/chat-hai.xhtpm"
PROJ="$HERE/ops/+x/chat_hai_projector.+x"
RENDER_OPS_DIR="$HOUSE_ROOT"/*.monads/*.livedesk-taskbar/ops
BIN="$(cd $RENDER_OPS_DIR && pwd)/+x/khtpm_core_render.+x"

[ -x "$BIN" ]  || (cd $RENDER_OPS_DIR && sh build_core_render.sh) || true
[ -x "$PROJ" ] || (cd "$HERE/ops" && sh build_chat_hai_projector.sh) || true
for f in "$BIN" "$PROJ" "$XHTPM"; do
    [ -e "$f" ] || { echo "chat-hai button-pal.sh: missing $f" >&2; exit 1; }
done

AUDIT_DIR="$HERE/pieces/audit"
mkdir -p "$AUDIT_DIR"

# Only our own renderer instance (this exact binary + template), plus
# any stray loop / new projector it may have left behind. pgrep -f
# matches the full argv, reliable even with the emoji-laden house path.
mine() { pgrep -f "khtpm_core_render\.\+x.*chat-hai\.xhtpm" 2>/dev/null || true; }
strays() {
    pgrep -f "chat-hai/ops/\+x/chat_hai_projector\.\+x" 2>/dev/null || true
    pgrep -f "chat-hai/ops/chat_hai_loop\.sh" 2>/dev/null || true
}

pids="$(mine; strays)"
pids="$(echo "$pids" | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "chat-hai button-pal.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(mine; strays)"
    pids="$(echo "$pids" | grep -v '^$' || true)"
    [ -n "$pids" ] && { echo "$pids" | xargs -r kill -KILL; sleep 1; }
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" \
    >"$AUDIT_DIR/chat-hai-pal.log" 2>&1 < /dev/null &
echo $! >> "$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

n="$(mine | grep -c . || true)"
if [ "$n" -ge 1 ] 2>/dev/null; then
    echo "chat-hai-pal launched (renderer pid $(mine | tr '\n' ' '), log=$AUDIT_DIR/chat-hai-pal.log)"
else
    echo "chat-hai button-pal.sh: FAILED to launch - check the log:" >&2
    cat "$AUDIT_DIR/chat-hai-pal.log" 2>/dev/null >&2
    exit 1
fi
