#!/bin/sh
# Open the game-dev office on the shared board. The picture is maps/game-dev-hq/office.
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; HOUSE="$(cd "$APP/../.." && pwd)"
export PCHQ_PKG="$APP/game-dev-hq-pchq"
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
[ -x "$APP/ops/+x/game_dev_hq.+x" ] || sh "$APP/ops/build_game_dev_hq.sh"
mkdir -p "$HERE/state"
"$APP/ops/+x/game_dev_hq.+x" exportbook >/dev/null
# One studio daemon writes ../state/ui.txt. A module of this window would use board/ as its app dir.
FUNNEL="$HOUSE/&.widgits/_shared-lib/kh_spawn.sh"; REG="$HOUSE/&.widgits/_shared-lib/ops/+x/proc_reg_op.+x"   # process registry: PROC-REGISTRY-STANDARD-AND-REFACTOR-PLAN-2026-11-02.md
ps -eo args | awk -v a="$APP" 'index($0,"game_dev_hq.+x") && index($0,a){f=1} END{exit !f}' || { KH_ROLE=daemon KH_APP=game-dev-hq sh "$FUNNEL" "$HOUSE" 0 game-dev-hq-daemon -- "$APP/ops/+x/game_dev_hq.+x" "$HOUSE" "$APP" >/dev/null 2>&1; }
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
# The world stops with its window (owner decision 2026-11-02): hand the daemon to the board window, so closing the window ends the daemon (and the game clock it owns) within ~35 s.
# Opt-in to a world that keeps running with the window closed: SECTION | keep_running | 1 in this app's toy.pdl (the row then stays lifetime keep, still listed in the registry).
KEEP="$(awk -F'|' '$2 ~ /keep_running/ {gsub(/[ \t]/, "", $3); print $3}' "$APP/toy.pdl" 2>/dev/null)"
WPID="$(cat "$PCHQ_PKG/pieces/system/board_window.pid" 2>/dev/null)"
DPID="$([ -x "$REG" ] && "$REG" "$HOUSE" list 2>/dev/null | awk -F' [|] ' '$10 == "game-dev-hq-daemon" {print $2}' | tail -1)"
if [ "$KEEP" != "1" ] && [ -x "$REG" ] && [ -n "$WPID" ] && [ -n "$DPID" ] && [ -d "/proc/$WPID" ] && [ -d "/proc/$DPID" ]; then
    "$REG" "$HOUSE" register "$DPID" owner "$WPID" daemon game-dev-hq game-dev-hq-daemon "$APP/ops/+x/game_dev_hq.+x $HOUSE $APP" >/dev/null 2>&1 || true
fi
# Declare the window the OWNER of the engine's detached stack (it respawns with the book, so rules, not a one-time adopt): this instance's session folder and the board-viewer
# session this window reports in state/ui.txt. Rules live in #.desktop/proc_adopt_rules.pdl, are applied by the guard every ~30 s, and die with the window.
if [ -x "$REG" ] && [ -n "$WPID" ] && [ -d "/proc/$WPID" ]; then
    "$REG" "$HOUSE" rule-add "$WPID" "$PCHQ_PKG/pieces/sessions/" engine game-dev-hq pchq-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" rule-add "$WPID" "@file:$HERE/state/ui.txt#bv_session" engine game-dev-hq bv-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" apply-rules >/dev/null 2>&1 || true
fi
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"
sleep 8; printf 'CONFIRM_START_MAP:game-dev-hq\n' >> "$INBOX"
