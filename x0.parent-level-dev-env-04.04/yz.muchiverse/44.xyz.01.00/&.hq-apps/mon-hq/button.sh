#!/bin/sh
# button.sh - launcher for the mon-hq "session monitor" window.
# Reached from the taskbar HQ menu ("mon" row). Same shape as
# &.hq-apps/stats-hq/button-pal.sh: resolve paths, single-instance
# guard, launch the shared khtpm_core_render on mon-hq.xhtpm. The
# xhtpm's own <module src="mon_refresh.sh"/> starts the refresh loop.
#
#   button.sh <house_root>
set -u
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "mon-hq: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/mon-hq.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"

chmod +x "$HERE/mon_scan.sh" "$HERE/mon_refresh.sh" 2>/dev/null || true
[ -x "$BIN" ] || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$BIN" ] || { echo "mon-hq: missing $BIN" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "mon-hq: missing $XHTPM" >&2; exit 1; }
mkdir -p "$HERE/state"

# single instance: drop any existing mon-hq renderer (and, via SIGTERM,
# the renderer reaps its own <module> refresh loop). Match on comm ==
# the renderer binary AND cmdline containing our xhtpm - never a bare
# "pgrep -f mon_refresh.sh", which self-matches any shell (incl. the
# caller) whose argv merely mentions the script (auto-memory:
# pkill-f-self-match-footgun).
for p in $(pgrep -f "khtpm_core_render\.\+x .*mon-hq\.xhtpm" 2>/dev/null || true); do
    [ "$p" = "$$" ] && continue
    comm=$(cat "/proc/$p/comm" 2>/dev/null || echo "")
    case "$comm" in sh|bash|dash|zsh) continue ;; esac
    if tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null | grep -q "mon-hq\.xhtpm"; then
        kill "$p" 2>/dev/null || true
        # its module children (mon_refresh.sh loop) go too
        for c in $(pgrep -P "$p" 2>/dev/null || true); do kill "$c" 2>/dev/null || true; done
    fi
done
sleep 1

# seed the view so the window has content on first paint
sh "$HERE/mon_scan.sh" publish "$HERE/state/ui.txt" >/dev/null 2>&1 || true

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo "mon-hq launched"
