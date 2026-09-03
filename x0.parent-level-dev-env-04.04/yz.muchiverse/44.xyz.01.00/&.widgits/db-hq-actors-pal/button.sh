#!/bin/sh
# button.sh - launch the PAL-driven Actors demo window.
#   button.sh <house_root>
# Starts ONLY the shared renderer; its launch_module() forks
# prisc+x on pal/actors_projector.pal (space-split <module> src).
set -e
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "db-hq-actors-pal: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
CHTPM="$HERE/db-hq-actors.chtpm"
BOOTSTRAP="$HERE/db-hq-actors.chtpm.bootstrap"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"
PRISC="$HOUSE_ROOT/&.widgits/_shared-lib/system/+x/prisc+x.+x"

[ -x "$BIN" ]   || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$PRISC" ] || sh "$HOUSE_ROOT/&.widgits/_shared-lib/ops/build_prisc.sh" || true
[ -x "$BIN" ]   || { echo "db-hq-actors-pal: missing $BIN" >&2; exit 1; }
[ -x "$PRISC" ] || { echo "db-hq-actors-pal: missing $PRISC" >&2; exit 1; }

mkdir -p "$HERE/state"
[ -f "$BOOTSTRAP" ] || { echo "db-hq-actors-pal: missing $BOOTSTRAP" >&2; exit 1; }
if [ ! -f "$CHTPM" ] || ! grep -q '<module' "$CHTPM" 2>/dev/null; then
    cp "$BOOTSTRAP" "$CHTPM"
fi

for p in $(pgrep -f "khtpm_core_render\.\+x .*db-hq-actors\.chtpm" 2>/dev/null || true) \
         $(pgrep -f "prisc\+x\.\+x .*actors_projector\.pal" 2>/dev/null || true); do
    kill "$p" 2>/dev/null || true
done
sleep 1

setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM" >/dev/null 2>&1 < /dev/null &
echo "db-hq-actors-pal launched (renderer + prisc+x projector)"
