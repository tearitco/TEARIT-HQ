#!/bin/sh
# open_board.sh - open the Teario-Make BOARD window (its own window, own header and left bar; not the rpg-edit editor). Phase P1 of the Teario window plan (BOOK 16.game TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN).
# Same shape as the rpg-edit open_board.sh (the standard: pc-hq instance init, the board launched with THIS folder's template, the book opened by widget commands, the window handed ownership of what it starts), without the editor's daemon, prefs and last-book logic.
# 1) the pc-hq instance of this app (data folder teario-make-pchq)  2) the game driver (game_core driver: publishes the stamp and game feeds into ../state/, takes the game bar presses)  3) the board window  4) open the book teario-make on the page play.
HERE="${TEARIO_BOARD_DIR:-$(cd "$(dirname "$0")" && pwd)}"; APP="$(dirname "$HERE")"; NAME="$(basename "$APP")"; HOUSE="$(cd "$APP/../.." && pwd)"   # TEARIO_BOARD_DIR: another Teario app (teario-play) opens its OWN window with this same launcher; its board/ folder holds its own template and window state
GATE="$HOUSE/&.widgits/_shared-lib/ops/+x/cpu_gate.+x"
[ -x "$GATE" ] && { "$GATE" "$HOUSE" check --wait 90 >/dev/null || { echo "open_board: the machine is too busy right now (cpu_gate); not relaunching" >&2; exit 1; }; }
mkdir -p "$APP/state/games/$NAME"   # the driver locks itself in the editor state folder: a new Teario app has none yet
export PCHQ_PKG="$APP/$NAME-pchq"
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
# the FACING (owner 2026-11-19, "player should be facing goal"): the engine default yaw 180 looks ACROSS a level that runs along x (first person shows only sky); yaw 90 looks along x, at the goal. default_yaw is read by the engine from the package arrow_config.txt (a key only this package sets; a board without it keeps 180). One idempotent line; the file itself is made by the instance init above.
AC="$PCHQ_PKG/pieces/system/arrow_config.txt"; if [ -f "$AC" ]; then if grep -q '^default_yaw=' "$AC"; then sed -i 's/^default_yaw=.*/default_yaw=90/' "$AC"; else printf 'default_yaw=90\n' >> "$AC"; fi; fi
# the GAME KEYS (option C, the R14 fix, D97): while the xelector holds a game-driven possession (People1), the engine key op forwards these codes (arrows 1000-1003, space 32) to <editor>/state/interact_relay.txt, which game_core tails. One idempotent row each, in the package keybinds.pdl.
KB="$PCHQ_PKG/pieces/system/keybinds.pdl"; if [ -f "$KB" ]; then for gk in left:1000 right:1001 up:1002 down:1003 jump:32; do grep -q "^GAMEKEY .*| ${gk#*:}\$" "$KB" || printf 'GAMEKEY      | %s | %s\n' "${gk%%:*}" "${gk#*:}" >> "$KB"; done; fi
# the stamp bank is the DEFAULT left bar: seed side_stamp=1 only when the file is missing (a user's own minimize is kept across launches)
[ -f "$HERE/state/sides.txt" ] || { if [ "$NAME" = teario-make ]; then printf 'side_stamp=1\n'; else printf 'side_stamp=\n'; fi > "$HERE/state/sides.txt"; }   # only the Make window has a stamp bank
# the ONE game driver (one instance: it locks itself). Registered through the spawn funnel (AGENTS.md: every process the house starts is recorded in #.desktop/proc_registry.pdl); handed to the window below so it ends with it.
FUNNEL="$HOUSE/&.widgits/_shared-lib/kh_spawn.sh"; REG="$HOUSE/&.widgits/_shared-lib/ops/+x/proc_reg_op.+x"; DRV="$HOUSE/&.widgits/_shared-lib/ops/+x/game_core.+x"
# is a driver of THIS window folder already registered and alive? The registry is the record (AGENTS.md: every process the house starts is a row), NOT a `ps | awk` scan of the arguments: that scan matched the awk process own command line and always answered yes (found 2026-11-19, the driver never started).
DPID0="$([ -x "$REG" ] && "$REG" "$HOUSE" list 2>/dev/null | awk -F' [|] ' -v n="$NAME-driver" '$10 == n {print $2}' | tail -1)"
{ [ -n "$DPID0" ] && [ -d "/proc/$DPID0" ]; } || {
    KH_ROLE=daemon KH_APP=$NAME sh "$FUNNEL" "$HOUSE" 0 $NAME-driver -- "$DRV" driver "$HOUSE" "$APP" "$APP" >/dev/null 2>&1; }
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
WPID="$(cat "$PCHQ_PKG/pieces/system/board_window.pid" 2>/dev/null)"
DPID="$([ -x "$REG" ] && "$REG" "$HOUSE" list 2>/dev/null | awk -F' [|] ' -v n="$NAME-driver" '$10 == n {print $2}' | tail -1)"
if [ -n "$WPID" ] && [ -n "$DPID" ] && [ -x "$REG" ] && [ -d "/proc/$WPID" ] && [ -d "/proc/$DPID" ]; then
    "$REG" "$HOUSE" register "$DPID" owner "$WPID" daemon $NAME $NAME-driver "$DRV driver $HOUSE $APP $APP" >/dev/null 2>&1 || true
fi
# open the book teario-make on its page play (the same two widget commands the editor sends; START_MAP finishes AFTER a SET_DESK sent in the same instant, so wait until the engine names the book, then set the desk)
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"; BOOK=$NAME; PAGE=play
sleep 8; printf 'CONFIRM_START_MAP:%s\n' "$BOOK" >> "$INBOX"
for _i in $(seq 1 20); do sleep 2; [ "$(sed -n 's/^map_id=//p' "$PCHQ_PKG/pieces/world_01/state.txt" 2>/dev/null | head -1)" = "$BOOK" ] && break; done
sleep 6; printf 'CONFIRM_SET_DESK:%s\n' "$PAGE" >> "$INBOX"
# the window owns the engine's detached stack: it ends with the window (two rules in #.desktop/proc_adopt_rules.pdl, applied by the registry pass)
if [ -x "$REG" ] && [ -n "$WPID" ] && [ -d "/proc/$WPID" ]; then
    "$REG" "$HOUSE" rule-add "$WPID" "$PCHQ_PKG/pieces/sessions/" engine $NAME pchq-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" rule-add "$WPID" "@file:$HERE/state/ui.txt#bv_session" engine $NAME bv-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" apply-rules >/dev/null 2>&1 || true
fi
