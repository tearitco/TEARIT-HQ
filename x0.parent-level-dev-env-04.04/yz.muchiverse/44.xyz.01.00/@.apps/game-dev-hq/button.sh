#!/bin/bash
# Open the game-dev-hq window. Usage: sh button.sh run
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
XHTPM="$SCRIPT_DIR/game-dev-hq.xhtpm"
[ "${1:-run}" = run ] || exit 0
mkdir -p "$SCRIPT_DIR/state"
[ -x "$SCRIPT_DIR/ops/+x/game_dev_hq.+x" ] || sh "$SCRIPT_DIR/ops/build_game_dev_hq.sh"
"$SCRIPT_DIR/ops/+x/game_dev_hq.+x" status >/dev/null
old="$(ps -eo pid,comm,args | awk -v x="$XHTPM" '$2=="khtpm_core_rend" && index($0,x){print $1}')"
[ -n "$old" ] && echo "$old" | xargs -r kill -TERM && sleep 1
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"$SCRIPT_DIR/state/window.log" 2>&1 < /dev/null &
printf '%s %s 0 0 game-dev-hq\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
disown 2>/dev/null || true
echo "game-dev-hq launched"
