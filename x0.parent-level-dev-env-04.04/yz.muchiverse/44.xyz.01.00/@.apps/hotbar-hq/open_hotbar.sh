#!/bin/sh
# macOS has no setsid binary (same convention as run_khtpm_strip.sh): expand to nothing there, keep real setsid on Linux.
SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""
# open_hotbar.sh <house_root> desk|pchq - open the hotbar window (single instance per mode).
# The window is a normal khtpm_core_render HQ window on hotbar-<mode>.xhtpm; its
# <module> starts ops/+x/hotbar_manager.+x, which publishes state/<mode>/ui.txt.
# Recorded in the proc ledger so a taskbar quit reaps it. Design:
# 18.pc-hq/CURSWORD-POSSESSION-DESIGN.md 5b/5e.
set -u
HOUSE_ROOT="${1:-}"; MODE="${2:-desk}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "open_hotbar: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
case "$MODE" in desk) ;; *) echo "open_hotbar: mode must be desk (pc-hq draws its hotbar inside the board window)" >&2; exit 1 ;; esac
HERE="$(cd "$(dirname "$0")" && pwd)"
OPS="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops"
BIN="$OPS/+x/khtpm_core_render.+x"
MGR="$HERE/ops/+x/hotbar_manager.+x"
XHTPM="$HERE/hotbar-$MODE.xhtpm"
[ -x "$BIN" ] || { echo "open_hotbar: missing $BIN (sh \$.crypts/button.sh build)" >&2; exit 1; }
[ -x "$MGR" ] || sh "$HERE/ops/build_hotbar_manager.sh" >/dev/null 2>&1
[ -x "$MGR" ] || { echo "open_hotbar: manager build failed" >&2; exit 1; }
mkdir -p "$HERE/state/$MODE"
# pids of THIS house's hotbar window and manager for this mode (a snapshot first, so the matcher is never in it; the house path keeps another
# checkout's hotbar out of it - the old pgrep -f killed that one too)
hb_pids() {     # argv[0] must BE the renderer / manager (a shell whose command line merely mentions them is not a match)
    ps -eo pid,args | awk -v h="$HOUSE_ROOT" -v m="$MODE" 'NR > 1 { p = $1; a0 = $2; $1 = ""; if (index($0, h) == 0) next;
        if (a0 ~ /khtpm_core_render\.\+x$/ && index($0, "hotbar-" m ".xhtpm")) print p; else if (a0 ~ /hotbar_manager\.\+x$/ && index($0, " " m)) print p }'
}
# --if-not-running (the autostart entry, boot.sh): a hotbar that is already up is LEFT ALONE. Without it every second start (a second `button.sh run`,
# autostart plus a relaunch, a double click) killed the good first hotbar and opened a new one, which looked like the desktop starting it twice
# (owner 2026-10-11). A lock (mkdir, with the owner pid) also stops two launches that race each other from both passing the check.
LOCK="$HERE/state/$MODE/.open.lock"
if [ "${3:-}" = "--if-not-running" ]; then
    if ! mkdir "$LOCK" 2>/dev/null; then
        lp="$(cat "$LOCK/pid" 2>/dev/null)"
        if [ -n "$lp" ] && kill -0 "$lp" 2>/dev/null; then echo "open_hotbar: another launch is in progress (pid $lp), leaving it"; exit 0; fi
        rm -rf "$LOCK"; mkdir "$LOCK" 2>/dev/null || exit 0     # a dead launch left it behind
    fi
    echo $$ > "$LOCK/pid"; trap 'rm -rf "$LOCK"' EXIT INT TERM
    if [ -n "$(hb_pids | head -1)" ]; then echo "open_hotbar: $MODE hotbar already running, leaving it"; exit 0; fi
fi
# --swap (the desktop restart, close_on_restart.pdl "swap:" row; owner 2026-10-12: the old hotbar vanished at restart and the new one only appeared seconds later, when the dock was ready): make before break.
# Wait for the dock like boot.sh, start the NEW window while the OLD one is still on screen, and only when the new window has drawn (its registry row exists) and had a moment to map at its docked spot,
# stop the old window + manager. A cold start with no old hotbar behaves like boot.sh.
SWAP_OLD=""
if [ "${3:-}" = "--swap" ]; then
    SWAP_OLD="$(hb_pids)"
    # (owner 2026-10-12: a second hotbar tab and window showed for a moment at every desktop start) a running hotbar that is already CURRENT is left alone: the window re-docks itself from the
    # manager's anchor vars, so a restart needs a second window only when the renderer, the manager or the template changed after that window started (/proc/<pid> is stamped at process start).
    if [ -n "$SWAP_OLD" ]; then
        STALE=0
        for p in $SWAP_OLD; do
            for f in "$BIN" "$MGR" "$XHTPM" "$HERE"/*.css "$HERE"/hotbar-*.xhtpm; do [ -e "$f" ] && [ "$f" -nt "/proc/$p" ] && STALE=1; done
        done
        if [ "$STALE" = 0 ]; then echo "open_hotbar: $MODE hotbar already running and current, leaving it"; exit 0; fi
    fi
    i=0; while [ ! -s "$HOUSE_ROOT/#.desktop/dock_stack/base.txt" ] && [ "$i" -lt 30 ]; do sleep 1; i=$((i+1)); done
else
# single instance: stop an older window + its manager for THIS mode (an explicit open, no flag, still restarts it)
for p in $(hb_pids); do kill "$p" 2>/dev/null || true; done
sleep 0.3
fi
rm -f "$HOUSE_ROOT"/#.desktop/hq_mapped_*.txt 2>/dev/null
$SETSID nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"/tmp/hotbar-$MODE.log" 2>&1 < /dev/null &
# real starttime/pgid so the C reaper does not skip it on quit/restart (a "0 0" line is ignored by its PID-reuse guard; the hotbar survived restarts because of that)
NEWPID="$!"
sh "$HOUSE_ROOT/&.widgits/_shared-lib/ops/proc_ledger_add.sh" "$HOUSE_ROOT" "$!" "hotbar-$MODE"
if [ -n "$SWAP_OLD" ]; then
    # one tab in the bottom bar at all times: as soon as the new window has registered its row, the old window is frozen (it stays on screen as it is), its row removed and the taskbar told
    i=0; while [ ! -e "$HOUSE_ROOT/#.desktop/livedesk_hq_windows_$NEWPID.txt" ] && kill -0 "$NEWPID" 2>/dev/null && [ "$i" -lt 50 ]; do sleep 0.1; i=$((i+1)); done
    for p in $SWAP_OLD; do kill -STOP "$p" 2>/dev/null; rm -f "$HOUSE_ROOT/#.desktop/livedesk_hq_windows_$p.txt"; echo "$p remove" >> "$HOUSE_ROOT/#.desktop/hq_windows_changed.txt"; done
    i=0; while [ ! -e "$HOUSE_ROOT/#.desktop/hq_mapped_$NEWPID.txt" ] && kill -0 "$NEWPID" 2>/dev/null && [ "$i" -lt 100 ]; do sleep 0.1; i=$((i+1)); done      # the renderer writes it right after XMapRaised
    sleep 0.3
    for p in $SWAP_OLD; do kill -CONT "$p" 2>/dev/null; kill "$p" 2>/dev/null || true; done      # a stopped process ignores TERM until it is resumed
fi
echo "open_hotbar: $MODE hotbar launched"
