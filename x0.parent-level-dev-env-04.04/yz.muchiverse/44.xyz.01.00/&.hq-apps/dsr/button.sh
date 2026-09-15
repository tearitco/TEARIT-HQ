#!/bin/sh
# button.sh - launch DSR (Desk Street Raider): a real X11-HQ window
# (dsr.xhtpm) driven by a real, separate, compiled manager
# (dsr_manager.c, same house convention db-hq-pal/chat-hai/network
# already use - a real process owns state, publishes state/ui.txt, the
# generic renderer draws it, zero new per-project C in the renderer).
#   button.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "dsr: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/dsr.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"
MGR="$HERE/ops/+x/dsr_manager.+x"

[ -x "$BIN" ] || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$MGR" ] || sh "$HERE/ops/build_dsr_manager.sh" || true
[ -x "$BIN" ] || { echo "dsr: missing $BIN" >&2; exit 1; }
[ -x "$MGR" ] || { echo "dsr: missing $MGR" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "dsr: missing $XHTPM" >&2; exit 1; }
mkdir -p "$HERE/state"

for p in $(pgrep -f "khtpm_core_render\.\+x .*dsr\.xhtpm" 2>/dev/null || true) \
         $(pgrep -f "dsr_manager\.\+x" 2>/dev/null || true); do
    kill "$p" 2>/dev/null || true
done
sleep 0.3

setsid nohup "$MGR" "$HOUSE_ROOT" >/dev/null 2>&1 < /dev/null &
sleep 0.2
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo "dsr launched (renderer + manager)"
