#!/bin/sh
# open_board.sh - open the Robot Trap Auto BOARD window (D113). The shape of teario-make/board/open_board.sh WITHOUT the game driver (RTA has no ticks: the desk is a projection of the ledger, rta_board_op).
# 1) the pc-hq instance of this app (data folder rta-board-pchq)  2) the config the life op reads to re-project after every verb (#.desktop/rta_board.pdl, opt-in marker file)  3) the first projection  4) the board window  5) open the book rta-board on its page board.
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; NAME="$(basename "$APP")"; HOUSE="$(cd "$APP/../.." && pwd)"
GATE="$HOUSE/&.widgits/_shared-lib/ops/+x/cpu_gate.+x"
[ -x "$GATE" ] && { "$GATE" "$HOUSE" check --wait 90 >/dev/null || { echo "open_board: the machine is too busy right now (cpu_gate); not relaunching" >&2; exit 1; }; }
mkdir -p "$APP/state"
export PCHQ_PKG="$APP/$NAME-pchq"
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
DESK="$PCHQ_PKG/pieces/system/maps/$NAME/board"; mkdir -p "$DESK"
[ -f "$PCHQ_PKG/pieces/system/maps/$NAME/game.pdl" ] || cp "$APP/game.pdl" "$PCHQ_PKG/pieces/system/maps/$NAME/game.pdl"
# the top-down view of a flat city: yaw 90 looks along x (the road runs east)
AC="$PCHQ_PKG/pieces/system/arrow_config.txt"; if [ -f "$AC" ]; then if grep -q '^default_yaw=' "$AC"; then sed -i 's/^default_yaw=.*/default_yaw=90/' "$AC"; else printf 'default_yaw=90\n' >> "$AC"; fi; fi
mkdir -p "$HERE/state"; [ -f "$HERE/state/sides.txt" ] || printf 'side_stamp=\n' > "$HERE/state/sides.txt"
# the projection config: rta_life_op finds it beside the ledger and runs rta_board_op after every accepted verb
D="$HOUSE/#.desktop"
printf 'BOARD | rules | %s\nBOARD | grid | %s\nBOARD | life | %s\nBOARD | desk | %s\nBOARD | ui | %s\nBOARD | data | %s\nBOARD | atlas | %s\nBOARD | entity | joe\nBOARD | car | car\nBOARD | png | %s\n' \
    "$HOUSE/@.apps/rta/rules.pdl" "$HOUSE/@.apps/rta/road.grid" "$D/rta_life_ledger.txt" "$DESK" "$APP/state/game_ui.txt" "$APP/board.pdl" "$APP/atlas.rgba" "$HOUSE/&.widgits/_shared-lib/ops/+x/raw_to_png.+x" > "$D/rta_board.pdl"
"$HOUSE/@.apps/rta/ops/+x/rta_board_op.+x" project "$D/rta_board.pdl" >/dev/null 2>&1 || { echo "open_board: the first projection failed" >&2; exit 1; }
FUNNEL="$HOUSE/&.widgits/_shared-lib/kh_spawn.sh"; REG="$HOUSE/&.widgits/_shared-lib/ops/+x/proc_reg_op.+x"
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
WPID="$(cat "$PCHQ_PKG/pieces/system/board_window.pid" 2>/dev/null)"
# open the book on its page (the same two widget commands the editor sends; wait until the engine names the book, then set the desk)
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"; BOOK=$NAME; PAGE=board
sleep 8; printf 'CONFIRM_START_MAP:%s\n' "$BOOK" >> "$INBOX"
for _i in $(seq 1 20); do sleep 2; [ "$(sed -n 's/^map_id=//p' "$PCHQ_PKG/pieces/world_01/state.txt" 2>/dev/null | head -1)" = "$BOOK" ] && break; done
sleep 6; printf 'CONFIRM_SET_DESK:%s\n' "$PAGE" >> "$INBOX"
# the window owns the engine's detached stack: it ends with the window
if [ -x "$REG" ] && [ -n "$WPID" ] && [ -d "/proc/$WPID" ]; then
    "$REG" "$HOUSE" rule-add "$WPID" "$PCHQ_PKG/pieces/sessions/" engine $NAME pchq-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" rule-add "$WPID" "@file:$HERE/state/ui.txt#bv_session" engine $NAME bv-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" apply-rules >/dev/null 2>&1 || true
fi
