#!/bin/sh
# button-pal.sh - PARALLEL launcher for the static-xhtpm stats-hq window
# (mirrors &.hq-apps/db-hq-pal/button.sh). The old open_stats_hq.sh +
# dashboard.chtpm (class="stats-hq") stay untouched as rollback.
#
#   button-pal.sh <house_root>
#
# Starts:
#   - stats_hq_manager.+x  (house-wide session scanner) if not running
#   - khtpm_core_render.+x  on stats-hq-pal.xhtpm ; its <module> forks
#     prisc+x on pal/stats_projector.pal
# class="stats-hq-pal" -> does NOT trip the renderer's g_is_stats_hq path.
set -e
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "stats-hq-pal: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/stats-hq-pal.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"
PRISC="$HOUSE_ROOT/&.widgits/_shared-lib/system/+x/prisc+x.+x"
MGR="$RENDER_OPS/+x/stats_hq_manager.+x"

[ -x "$BIN" ]   || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$PRISC" ] || sh "$HOUSE_ROOT/&.widgits/_shared-lib/ops/build_prisc.sh" || true
[ -x "$MGR" ]   || (cd "$RENDER_OPS" && sh build_stats_hq_manager.sh) || true
[ -x "$BIN" ]   || { echo "stats-hq-pal: missing $BIN" >&2; exit 1; }
[ -x "$PRISC" ] || { echo "stats-hq-pal: missing $PRISC" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "stats-hq-pal: missing $XHTPM" >&2; exit 1; }
mkdir -p "$HERE/state"

for p in $(pgrep -f "khtpm_core_render\.\+x .*stats-hq-pal\.xhtpm" 2>/dev/null || true) \
         $(pgrep -f "prisc\+x\.\+x .*stats_projector\.pal" 2>/dev/null || true); do
    kill "$p" 2>/dev/null || true
done
sleep 1

if [ -x "$MGR" ] && ! pgrep -f "stats_hq_manager\.\+x" >/dev/null 2>&1; then
    setsid nohup "$MGR" "$HOUSE_ROOT" >/dev/null 2>&1 < /dev/null &
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo "stats-hq-pal launched (renderer + prisc+x projector)"
