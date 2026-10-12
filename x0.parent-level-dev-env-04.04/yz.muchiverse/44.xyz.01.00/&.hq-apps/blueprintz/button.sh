#!/bin/sh
# blueprintz launcher (same shape as sql-hq/button.sh): sh button.sh <house_root>
# One instance: an already-open blueprintz window is closed first. The old window is found by an exact process-name scan, never `pgrep -f` / `pkill -f` (that pattern kills the shell that runs it).
set -e
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "blueprintz: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/blueprintz.xhtpm"
BIN="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
[ -x "$BIN" ]   || (cd "$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops" && sh build_core_render.sh) || true
[ -x "$BIN" ]   || { echo "blueprintz: missing $BIN" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "blueprintz: missing $XHTPM" >&2; exit 1; }
for p in $(ps -eo pid=,comm=,args= | awk '$2 ~ /^khtpm_core_rend/ && index($0, "blueprintz.xhtpm") && $1 != '"$$"' {print $1}'); do
    kill "$p" 2>/dev/null || true
done
sleep 1
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo "blueprintz launched"
