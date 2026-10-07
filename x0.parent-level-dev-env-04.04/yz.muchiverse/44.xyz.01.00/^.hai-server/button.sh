#!/bin/sh
# ^.hai-server/button.sh start|stop|status|once   - run the phone router loop (router.pal -> server_route_op every 2 s).
#
# SAFETY: the live server dir routes the REAL entities' phones. start refuses on it unless HAI_ROUTER_LIVE_OK=1 is set (owner's OK, Q009). To test, point it at a copy:
#     HAI_SERVER_DIR=/path/to/copy/server  [HAI_ROUTER_OPS=/path/to/ops/+x]  sh button.sh start
# stop kills by the pidfile (never pkill -f: that pattern self-matches and kills the caller's shell - house footgun).
# Files (in the server dir, all gitignored): router.run.pal (generated), router.pid, router.log, system/prisc+x (compiled here, same as world-manager).
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/.." && pwd)"
SERVER_DIR="${HAI_SERVER_DIR:-$SCRIPT_DIR}"
OPS="${HAI_ROUTER_OPS:-$HOUSE/&.widgits/_shared-lib/ops/+x}"
PIDFILE="$SERVER_DIR/router.pid"
alive() { [ -f "$PIDFILE" ] && p="$(cat "$PIDFILE" 2>/dev/null)" && [ -n "$p" ] && kill -0 "$p" 2>/dev/null; }
case "${1:-status}" in
    start)
        if [ "$SERVER_DIR" = "$SCRIPT_DIR" ] && [ "${HAI_ROUTER_LIVE_OK:-}" != "1" ]; then
            echo "refused: this would route the LIVE phones. Needs the owner's OK: set HAI_ROUTER_LIVE_OK=1 (or test on a copy with HAI_SERVER_DIR=...)" >&2; exit 1
        fi
        [ -x "$OPS/server_route_op.+x" ] || { echo "missing $OPS/server_route_op.+x (sh '&.widgits/_shared-lib/ops/build_phone_ensure_op.sh')" >&2; exit 1; }
        [ -f "$SERVER_DIR/phones.index" ] || { echo "missing $SERVER_DIR/phones.index" >&2; exit 1; }
        alive && { echo "router already running (pid $(cat "$PIDFILE"))"; exit 0; }
        mkdir -p "$SERVER_DIR/system"
        gcc -O0 -std=c11 -w -o "$SERVER_DIR/system/prisc+x" "$HOUSE/&.widgits/_shared-lib/system/prisc+x.c" || { echo "prisc+x build failed" >&2; exit 1; }
        # fill @OPS@ with plain awk string ops: POSIX (this runs under dash too), and the path contains '&', which sed would expand
        awk -v ops="$OPS" '{ while ((i = index($0, "@OPS@")) > 0) $0 = substr($0, 1, i - 1) ops substr($0, i + 5); print }' "$SCRIPT_DIR/router.pal" > "$SERVER_DIR/router.run.pal"
        cd "$SERVER_DIR" || exit 1
        setsid nohup ./system/prisc+x ./router.run.pal >> router.log 2>&1 < /dev/null &
        echo $! > "$PIDFILE"
        [ "$SERVER_DIR" = "$SCRIPT_DIR" ] && sh "$HOUSE/&.widgits/_shared-lib/ops/proc_ledger_add.sh" "$HOUSE" "$(cat "$PIDFILE")" hai-router   # live only: a taskbar quit reaps it (real starttime: the reaper skips "0 0" lines)
        echo "router started (pid $(cat "$PIDFILE")), server dir $SERVER_DIR"
        ;;
    stop)
        if alive; then p="$(cat "$PIDFILE")"; kill "$p" 2>/dev/null; sleep 0.3; kill -0 "$p" 2>/dev/null && kill -9 "$p" 2>/dev/null; rm -f "$PIDFILE"; echo "router stopped (pid $p)"
        else rm -f "$PIDFILE"; echo "router not running"; fi
        ;;
    status)
        if alive; then echo "router running (pid $(cat "$PIDFILE")), server dir $SERVER_DIR"; else echo "router not running"; fi
        ;;
    once)
        "$OPS/server_route_op.+x" "$SERVER_DIR"
        ;;
    *) echo "usage: button.sh start|stop|status|once" >&2; exit 2 ;;
esac
