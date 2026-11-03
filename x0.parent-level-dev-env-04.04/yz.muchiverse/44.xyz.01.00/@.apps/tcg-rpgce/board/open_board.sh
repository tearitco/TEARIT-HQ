#!/bin/sh
# open_board.sh - open the tcg-rpgce BOARD window (tcg-rpgce as a module of the pc-hq board engine; the standalone window stays: sh ../button.sh run).
# 1) export the rooms as the book maps/tcg-rpgce (map.png, events.txt, bar.txt, ascii_legend.txt, 3D set)  2) launch the pc-hq board with THIS folder's template
# (open_pchq_board.sh takes PCHQ_BOARD_TPL; the template must be named pchq-board.xhtpm)  3) tell the board to open the tcg-rpgce book.
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; HOUSE="$(cd "$APP/../.." && pwd)"
# tcg-rpgce runs on its OWN pc-hq instance (PCHQ-SESSIONS-DESIGN.md): data folder <app>/tcg-rpgce-pchq (world, hero, xelector, maps, engine sessions), programs shared from @.apps/piececraft-hq. The launcher
# creates the folder when it is missing and only ever replaces THIS instance's board window, never another game's.
export PCHQ_PKG="$APP/tcg-rpgce-pchq"
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
mkdir -p "$HERE/state"; [ -f "$HERE/state/sides.txt" ] || printf "side_left=1\nside_right=1\n" > "$HERE/state/sides.txt"
"$APP/ops/+x/tcg_rpgce.+x" export3d >/dev/null 2>&1
# the ONE tcg-rpgce daemon (walks the pets, chat, clock, writes ../state/ui.txt): not a <module> of this window, a module runs with THIS folder as its app dir and would start a second house in board/state
FUNNEL="$HOUSE/&.widgits/_shared-lib/kh_spawn.sh"; REG="$HOUSE/&.widgits/_shared-lib/ops/+x/proc_reg_op.+x"   # process registry: PROC-REGISTRY-STANDARD-AND-REFACTOR-PLAN-2026-11-02.md
ps -eo stat,comm | awk '$2=="tcg_rpgce.+x" && $1 !~ /Z/{f=1} END{exit !f}' || { KH_ROLE=daemon KH_APP=tcg-rpgce sh "$FUNNEL" "$HOUSE" 0 tcg-rpgce-daemon -- "$APP/ops/+x/tcg_rpgce.+x" "$HOUSE" "$APP" >/dev/null 2>&1; }
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
# The world stops with its window (owner decision 2026-11-02): hand the daemon to the board window, so closing the window ends the daemon (and the game clock it owns) within ~35 s.
# Opt-in to a world that keeps running with the window closed: SECTION | keep_running | 1 in this app's toy.pdl (the row then stays lifetime keep, still listed in the registry).
KEEP="$(awk -F'|' '$2 ~ /keep_running/ {gsub(/[ \t]/, "", $3); print $3}' "$APP/toy.pdl" 2>/dev/null)"
WPID="$(cat "$PCHQ_PKG/pieces/system/board_window.pid" 2>/dev/null)"
DPID="$([ -x "$REG" ] && "$REG" "$HOUSE" list 2>/dev/null | awk -F' [|] ' '$10 == "tcg-rpgce-daemon" {print $2}' | tail -1)"
if [ "$KEEP" != "1" ] && [ -x "$REG" ] && [ -n "$WPID" ] && [ -n "$DPID" ] && [ -d "/proc/$WPID" ] && [ -d "/proc/$DPID" ]; then
    "$REG" "$HOUSE" register "$DPID" owner "$WPID" daemon tcg-rpgce tcg-rpgce-daemon "$APP/ops/+x/tcg_rpgce.+x $HOUSE $APP" >/dev/null 2>&1 || true
fi
# Declare the window the OWNER of the engine's detached stack (it respawns with the book, so rules, not a one-time adopt): this instance's session folder and the board-viewer
# session this window reports in state/ui.txt. Rules live in #.desktop/proc_adopt_rules.pdl, are applied by the guard every ~30 s, and die with the window.
if [ -x "$REG" ] && [ -n "$WPID" ] && [ -d "/proc/$WPID" ]; then
    "$REG" "$HOUSE" rule-add "$WPID" "$PCHQ_PKG/pieces/sessions/" engine tcg-rpgce pchq-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" rule-add "$WPID" "@file:$HERE/state/ui.txt#bv_session" engine tcg-rpgce bv-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" apply-rules >/dev/null 2>&1 || true
fi
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"
sleep 8; printf 'CONFIRM_START_MAP:tcg-rpgce\n' >> "$INBOX"
