#!/bin/sh
# blueprintz launcher. Usage: sh button.sh <house_root>   (the taskbar's db menu) | sh button.sh run | stop   (house derived from this folder)
# One instance: an already-open blueprintz window is closed first (found by its pid file, then by an exact process-name scan, never `pgrep -f`/`pkill -f`).
# The window's manager (blueprintz_manager) is forked by the renderer from the module tag; this script only builds what is missing and starts the renderer.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
case "${1:-run}" in
  run|stop|"") ACTION="${1:-run}"; HOUSE_ROOT="$(cd "$HERE/../.." && pwd)" ;;
  *) ACTION=run; HOUSE_ROOT="$1"; [ -d "$HOUSE_ROOT" ] || { echo "blueprintz: need house_root as argv[1]" >&2; exit 1; }; HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)" ;;
esac
XHTPM="$HERE/blueprintz.xhtpm"
BIN="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
mkdir -p "$HERE/state"
stop_old() {
    p="$(cat "$HERE/state/renderer.pid" 2>/dev/null || true)"
    [ -n "$p" ] && kill -0 "$p" 2>/dev/null && kill -TERM "$p" 2>/dev/null || true
    rm -f "$HERE/state/renderer.pid"
    for p in $(ps -eo pid=,comm=,args= | awk '$2 ~ /^khtpm_core_rend/ && index($0, "blueprintz.xhtpm") && $1 != '"$$"' {print $1}'); do
        kill "$p" 2>/dev/null || true
    done
}
if [ "$ACTION" = "stop" ]; then stop_old; echo "blueprintz stopped"; exit 0; fi
[ -x "$BIN" ]   || (cd "$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops" && sh build_core_render.sh) || true
[ -x "$BIN" ]   || { echo "blueprintz: missing $BIN" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "blueprintz: missing $XHTPM" >&2; exit 1; }
{ [ -x "$HERE/+x/blueprintz_manager.+x" ] && [ -x "$HERE/ops/+x/bp_ctl.+x" ] && [ -x "$HERE/ops/+x/bp_scan.+x" ] && [ -x "$HERE/ops/+x/bp_req.+x" ]; } || sh "$HERE/ops/build.sh" || true
[ -x "$HERE/+x/blueprintz_manager.+x" ] || { echo "blueprintz: build failed" >&2; exit 1; }
stop_old
sleep 1
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo $! > "$HERE/state/renderer.pid"
echo "blueprintz launched (renderer pid $(cat "$HERE/state/renderer.pid"))"
