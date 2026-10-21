#!/bin/bash
# Open Casino Island. Does not touch the shooter window.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
XHTPM="$SCRIPT_DIR/bet-rpg.xhtpm"
[ "${1:-run}" = run ] || exit 0
mkdir -p "$SCRIPT_DIR/state" "$SCRIPT_DIR/audit"
[ -x "$SCRIPT_DIR/ops/+x/bet_rpg.+x" ] || sh "$SCRIPT_DIR/ops/build_bet_rpg.sh"
"$SCRIPT_DIR/ops/+x/bet_rpg.+x" status >/dev/null
old="$(ps -eo pid,comm,args | awk -v x="$XHTPM" '$2=="khtpm_core_rend" && index($0,x){print $1}')"
[ -n "$old" ] && echo "$old" | xargs -r kill -TERM && sleep 1
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"$SCRIPT_DIR/audit/bet-rpg.log" 2>&1 < /dev/null &
printf '%s %s 0 0 bet-rpg\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 2
echo "bet-rpg launched"
