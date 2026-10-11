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
# single instance: stop an older window + its manager for THIS mode
for p in $(pgrep -f "khtpm_core_render\.\+x .*hotbar-$MODE\.xhtpm" 2>/dev/null || true) \
         $(pgrep -f "hotbar_manager\.\+x .*$MODE" 2>/dev/null || true); do kill "$p" 2>/dev/null || true; done
sleep 0.3
$SETSID nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"/tmp/hotbar-$MODE.log" 2>&1 < /dev/null &
# real starttime/pgid so the C reaper does not skip it on quit/restart (a "0 0" line is ignored by its PID-reuse guard; the hotbar survived restarts because of that)
sh "$HOUSE_ROOT/&.widgits/_shared-lib/ops/proc_ledger_add.sh" "$HOUSE_ROOT" "$!" "hotbar-$MODE"
echo "open_hotbar: $MODE hotbar launched"
