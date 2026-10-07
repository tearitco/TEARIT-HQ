#!/bin/sh
# open_board.sh <house_root> <name> [close] - open (or close) the board window board-<name>.xhtpm. Single instance per board: the pid is kept in state/<name>/window.pid
# (never pkill -f: that pattern self-matches and kills the caller's shell). Recorded in the proc ledger so a taskbar quit reaps it. Moves/clicks nothing else.
set -u
HOUSE_ROOT="${1:-}"; NAME="${2:-}"; ACTION="${3:-open}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] && [ -n "$NAME" ] || { echo "usage: open_board.sh <house_root> <name> [close]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BOARD_RENDERER:-$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x}"; XHTPM="$HERE/board-$NAME.xhtpm"; PIDF="$HERE/state/$NAME/window.pid"
[ -x "$BIN" ] || { echo "missing $BIN" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "no such board: $NAME (sh ops/new_board.sh $NAME)" >&2; exit 1; }
alive() { [ -f "$PIDF" ] && p="$(cat "$PIDF" 2>/dev/null)" && [ -n "$p" ] && kill -0 "$p" 2>/dev/null; }
if [ "$ACTION" = close ]; then alive && { kill "$(cat "$PIDF")"; echo "board $NAME closed"; } || echo "board $NAME not open"; rm -f "$PIDF"; exit 0; fi
alive && { echo "board $NAME already open (pid $(cat "$PIDF"))"; exit 0; }
ops_x="$HERE/ops/+x/board_vars_op.+x"; [ -x "$ops_x" ] || sh "$HERE/ops/build_board_vars_op.sh" >/dev/null 2>&1
mkdir -p "$HERE/state/$NAME"
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"/tmp/board-$NAME.log" 2>&1 < /dev/null &
echo $! > "$PIDF"
printf '%s %s 0 0 board-%s\n' "$(cat "$PIDF")" "$(cat "$PIDF")" "$NAME" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
echo "board $NAME opened (pid $(cat "$PIDF"))"
