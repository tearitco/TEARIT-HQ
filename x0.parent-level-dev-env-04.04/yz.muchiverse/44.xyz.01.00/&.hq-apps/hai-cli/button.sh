#!/bin/bash
# button.sh - open hai-cli (the hai FSM chat window, W0: it follows the chat ledger) as its own detached X11 window.
# Usage: sh button.sh run | stop      Menu: H-AI > hai-cli (livedesk_taskbar.pdl ai_menu_12).
# This folder is only the launcher; the window, ops and data are the hai package named in hai_home.pdl.
# Renderer pid is kept in state/renderer.pid; stop signals exactly that pid (no pgrep -f).
set -e
mkdir -p "$(dirname "$0")/state"; exec >"$(dirname "$0")/state/launch.log" 2>&1   # last launch only (overwritten each run: capped by construction)
set -x
ACTION="${1:-run}"
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$HERE/../.." && pwd)"
HAI_HOME="$(sed -n 's/^HAI_HOME[ ]*|[ ]*//p' "$HERE/hai_home.pdl" | head -1)"
BIN="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
XHTPM="$HAI_HOME/window/hai.xhtpm"
mkdir -p "$HERE/state"
stop_old() {
    p="$(cat "$HERE/state/renderer.pid" 2>/dev/null || true)"
    [ -n "$p" ] && kill -0 "$p" 2>/dev/null && kill -TERM "$p" 2>/dev/null || true
    rm -f "$HERE/state/renderer.pid"
}
if [ "$ACTION" = "stop" ]; then stop_old; echo "hai-cli stopped"; exit 0; fi
[ -n "$HAI_HOME" ] && [ -f "$XHTPM" ] || { echo "hai-cli: no window at '$XHTPM' (fix hai_home.pdl)" >&2; exit 1; }
[ -x "$BIN" ] || { echo "hai-cli: missing renderer $BIN" >&2; exit 1; }
[ -x "$HAI_HOME/window/bin/hai_manager" ] || { echo "hai-cli: build the manager first (header of window/hai_manager.c)" >&2; exit 1; }
stop_old
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo $! > "$HERE/state/renderer.pid"
printf '%s %s 0 0 hai-cli\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 2
if kill -0 "$(cat "$HERE/state/renderer.pid")" 2>/dev/null; then echo "hai-cli launched (renderer pid $(cat "$HERE/state/renderer.pid"))"; else echo "hai-cli: failed to start" >&2; exit 1; fi
