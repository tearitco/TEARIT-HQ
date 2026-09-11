#!/bin/sh
# button.sh - launcher for the proc-mon "session monitor" window.
# Reached from the taskbar HQ menu ("mon" row). Same shape as
# &.hq-apps/stats-hq/button-pal.sh: resolve paths, single-instance
# guard, launch the shared khtpm_core_render on proc-mon.xhtpm. The
# xhtpm's own <module src="mon_refresh.sh"/> starts the refresh loop.
#
# REAL FIX 2026-09-11 (direct live report: "i clicked 'mon' and used
# nav. it didn't open. was it renamed or what?") - correct: this app
# was renamed mon-hq -> proc-mon earlier this same session (5bfbe8c6),
# which git mv'd this file (and mon-hq.xhtpm/.css) to their new
# names, but never actually updated THIS file's own internal string
# references to the old name - `git log --all -- button.sh` shows
# zero commits since the rename ever touched its content. Same real
# gap as *.livedesk-taskbar/ops/open_mon.sh, fixed in the same pass.
#
#   button.sh <house_root>
set -u
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "proc-mon: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/proc-mon.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"

chmod +x "$HERE/mon_scan.sh" "$HERE/mon_refresh.sh" 2>/dev/null || true
[ -x "$BIN" ] || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$BIN" ] || { echo "proc-mon: missing $BIN" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "proc-mon: missing $XHTPM" >&2; exit 1; }
mkdir -p "$HERE/state"

# single instance: drop any existing proc-mon renderer (and, via
# SIGTERM, the renderer reaps its own <module> refresh loop). Match on
# comm == the renderer binary AND cmdline containing our xhtpm - never
# a bare "pgrep -f mon_refresh.sh", which self-matches any shell (incl.
# the caller) whose argv merely mentions the script (auto-memory:
# pkill-f-self-match-footgun).
KILLED=0
for p in $(pgrep -f "khtpm_core_render\.\+x .*proc-mon\.xhtpm" 2>/dev/null || true); do
    [ "$p" = "$$" ] && continue
    comm=$(cat "/proc/$p/comm" 2>/dev/null || echo "")
    case "$comm" in sh|bash|dash|zsh) continue ;; esac
    if tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null | grep -q "proc-mon\.xhtpm"; then
        kill "$p" 2>/dev/null || true
        # its module children (mon_refresh.sh loop) go too
        for c in $(pgrep -P "$p" 2>/dev/null || true); do kill "$c" 2>/dev/null || true; done
        KILLED=1
    fi
done
# REAL FIX 2026-09-11, direct live report ("this window seems to wait
# till it runs script to open. that makes it feel really slow... cant
# it open first, say 'scanning'?"). Two real, stacked delays were here
# before the window ever mapped:
#   1. an UNCONDITIONAL `sleep 1`, even on a totally fresh launch with
#      nothing to kill above - now only sleeps if a real instance was
#      actually killed (settling time for ITS OWN teardown, not a
#      blanket tax on every launch).
#   2. a full synchronous `mon_scan.sh publish` (~1.5s of ps/awk/proc
#      reads per mon_refresh.sh's own header comment) - REDUNDANT:
#      mon_refresh.sh's <module> loop (started by the renderer itself,
#      right below) does its own `mon_scan.sh publish` as the very
#      FIRST thing it does, before its first sleep - so this was
#      running the same real scan twice, and blocking the window from
#      opening at all for the first one. Real fix: write a real,
#      instant "scanning" placeholder ONLY if state/ui.txt doesn't
#      already exist (a re-open after a prior real scan shows that
#      stale-but-real data immediately instead of blanking first) -
#      the window maps right away, mon_refresh.sh's own first loop
#      iteration replaces the placeholder with real data within ~1.5s
#      regardless of whether this ran.
[ "$KILLED" = 1 ] && sleep 1
if [ ! -f "$HERE/state/ui.txt" ]; then
    mkdir -p "$HERE/state"
    tmp="$HERE/state/ui.txt.tmp.$$"
    {
        echo "scan_time=(scanning...)"
        echo "house_root=$HOUSE_ROOT"
        echo "rows_count=0"
        echo "good_count=0"
        echo "bad_count=0"
        echo "stray_count=0"
        echo "bad_pids="
        echo "stray_pids="
        echo "no_good=1"
        echo "no_bad=1"
        echo "clean=1"
    } > "$tmp"
    mv -f "$tmp" "$HERE/state/ui.txt"
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo "proc-mon launched"
