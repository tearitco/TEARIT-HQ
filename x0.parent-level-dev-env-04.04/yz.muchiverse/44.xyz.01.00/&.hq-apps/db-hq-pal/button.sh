#!/bin/sh
# button.sh - launch db-hq-pal: the 15-tab RPG-Maker database as a
# STATIC x11-hq template (dashboard.xhtpm) driven by a PAL projector
# (pal/dbhq_projector.pal via prisc+x). class="db-hq-pal" so it does
# NOT trip the renderer's built-in g_is_db_hq C path.
#   button.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "db-hq-pal: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/dashboard.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"
PRISC="$HOUSE_ROOT/&.widgits/_shared-lib/system/+x/prisc+x.+x"

[ -x "$BIN" ]   || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$PRISC" ] || sh "$HOUSE_ROOT/&.widgits/_shared-lib/ops/build_prisc.sh" || true
[ -x "$BIN" ]   || { echo "db-hq-pal: missing $BIN" >&2; exit 1; }
[ -x "$PRISC" ] || { echo "db-hq-pal: missing $PRISC" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "db-hq-pal: missing $XHTPM" >&2; exit 1; }
mkdir -p "$HERE/state"

for p in $(pgrep -f "khtpm_core_render\.\+x .*dashboard\.xhtpm" 2>/dev/null || true) \
         $(pgrep -f "prisc\+x\.\+x .*dbhq_projector\.pal" 2>/dev/null || true); do
    kill "$p" 2>/dev/null || true
done
sleep 1

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo "db-hq-pal launched (renderer + prisc+x projector, 15 tabs)"
