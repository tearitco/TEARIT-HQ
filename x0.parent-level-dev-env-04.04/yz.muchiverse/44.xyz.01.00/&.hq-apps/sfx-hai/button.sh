#!/bin/bash
# button.sh - launch sfx-hai as its own detached X11 window. Usage: sh button.sh run | stop   (house root is derived: this folder is house/&.hq-apps/sfx-hai).
# Renderer pid is kept in state/renderer.pid; stop/restart signal exactly that pid and the manager pid in state/manager.pid (no pgrep -f).
set -e
ACTION="${1:-run}"
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$HERE/../.." && pwd)"
BIN="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
XHTPM="$HERE/sfx-hai.xhtpm"
mkdir -p "$HERE/state" "$HERE/audit"
stop_old() {
    for f in renderer manager; do
        p="$(cat "$HERE/state/$f.pid" 2>/dev/null || true)"
        [ -n "$p" ] && kill -0 "$p" 2>/dev/null && kill -TERM "$p" 2>/dev/null || true
        rm -f "$HERE/state/$f.pid"
    done
}
if [ "$ACTION" = "stop" ]; then stop_old; echo "sfx-hai stopped"; exit 0; fi
[ -x "$BIN" ] || (cd "$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops" && sh build_core_render.sh) || true
[ -x "$BIN" ] || { echo "sfx-hai: missing renderer $BIN" >&2; exit 1; }
[ -x "$HERE/+x/sfx_hai_manager.+x" ] && [ -x "$HERE/ops/+x/sfx_hai_req.+x" ] || (cd "$HERE" && sh build.sh) || true
[ -x "$HERE/+x/sfx_hai_manager.+x" ] || { echo "sfx-hai: build failed" >&2; exit 1; }
stop_old
sleep 1
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"$HERE/audit/sfx-hai.log" 2>&1 < /dev/null &
echo $! > "$HERE/state/renderer.pid"
printf '%s %s 0 0 sfx-hai\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 2
if kill -0 "$(cat "$HERE/state/renderer.pid")" 2>/dev/null; then echo "sfx-hai launched (renderer pid $(cat "$HERE/state/renderer.pid"))"; else echo "sfx-hai: failed, see $HERE/audit/sfx-hai.log" >&2; cat "$HERE/audit/sfx-hai.log" >&2; exit 1; fi
