#!/bin/sh
# button.sh - launcher for the h-ai-lab window. Reached from the
# taskbar's real 14.h-ai dropdown ("h-ai-lab" row). Same shape as
# &.hq-apps/proc-mon/button.sh: resolve paths, single-instance guard,
# instant placeholder so the window never blocks on the first scan,
# launch the shared khtpm_core_render on h-ai-lab.xhtpm. The xhtpm's
# own <module src="ai_lab_refresh.sh"/> starts the refresh loop.
#
#   button.sh <house_root>
set -u
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "h-ai-lab: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/h-ai-lab.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"

chmod +x "$HERE/ai_lab_scan.sh" "$HERE/ai_lab_refresh.sh" "$HOUSE_ROOT/&.widgits/ai-lab/ops/ai_registry.sh" 2>/dev/null || true
[ -x "$BIN" ] || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$BIN" ] || { echo "h-ai-lab: missing $BIN" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "h-ai-lab: missing $XHTPM" >&2; exit 1; }
mkdir -p "$HERE/state"

# single instance: drop any existing h-ai-lab renderer (and, via
# SIGTERM, the renderer reaps its own <module> refresh loop) - same
# real match convention as proc-mon/button.sh (comm == the renderer
# binary AND cmdline containing our xhtpm, never a bare pgrep -f on
# the script name, which self-matches any shell mentioning it).
KILLED=0
for p in $(pgrep -f "khtpm_core_render\.\+x .*h-ai-lab\.xhtpm" 2>/dev/null || true); do
    [ "$p" = "$$" ] && continue
    comm=$(cat "/proc/$p/comm" 2>/dev/null || echo "")
    case "$comm" in sh|bash|dash|zsh) continue ;; esac
    if tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null | grep -q "h-ai-lab\.xhtpm"; then
        kill "$p" 2>/dev/null || true
        for c in $(pgrep -P "$p" 2>/dev/null || true); do kill "$c" 2>/dev/null || true; done
        KILLED=1
    fi
done
[ "$KILLED" = 1 ] && sleep 1

# instant, real placeholder - same "never block window-open on a
# synchronous first-scan" rule CENTROID_GOLD_STD.md item 10 already
# states, ONLY written if no prior real scan exists yet (a re-open
# shows stale-but-real data immediately, not a blank flash).
if [ ! -f "$HERE/state/ui.txt" ]; then
    tmp="$HERE/state/ui.txt.tmp.$$"
    {
        echo "scan_time=(scanning...)"
        echo "n_instances=0"
        echo "no_instances=1"
        echo "has_detail=0"
        echo "no_detail=1"
        echo "detail_text=(select an instance from the list)"
    } > "$tmp"
    mv -f "$tmp" "$HERE/state/ui.txt"
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo "h-ai-lab launched"
