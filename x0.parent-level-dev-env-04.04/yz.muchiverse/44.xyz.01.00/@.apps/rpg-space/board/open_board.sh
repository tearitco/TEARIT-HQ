#!/bin/sh
# open_board.sh - open the rpg-space BOARD window (rpg-space on the shared pets-like core, as a module of the pc-hq board engine; first slice of this launcher is in git: da8fc7da9).
# 1) export the rooms as the book maps/rpg-space (map.png, events.txt, bar.txt, ascii_legend.txt, 3D set)  2) launch the pc-hq board with THIS folder's template
# (open_pchq_board.sh takes PCHQ_BOARD_TPL; the template must be named pchq-board.xhtpm)  3) tell the board to open the rpg-space book.
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; HOUSE="$(cd "$APP/../.." && pwd)"
# rpg-space runs on its OWN pc-hq instance (PCHQ-SESSIONS-DESIGN.md): data folder <app>/rpg-space-pchq (world, hero, xelector, maps, engine sessions), programs shared from @.apps/piececraft-hq. The launcher
# creates the folder when it is missing and only ever replaces THIS instance's board window, never another game's.
[ -x "$APP/ops/+x/rpg_space.+x" ] && [ -x "$APP/ops/+x/dust_gen.+x" ] || sh "$APP/ops/build_rpg_space.sh" >/dev/null || { echo "rpg-space: build failed" >&2; exit 1; }
export PCHQ_PKG="$APP/rpg-space-pchq"
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
mkdir -p "$HERE/state"; [ -f "$HERE/state/sides.txt" ] || printf "side_left=1\nside_right=1\n" > "$HERE/state/sides.txt"
"$APP/ops/+x/rpg_space.+x" export3d >/dev/null 2>&1
# the ONE rpg-space daemon (walks the pets, chat, clock, writes ../state/ui.txt): not a <module> of this window, a module runs with THIS folder as its app dir and would start a second house in board/state
ps -eo stat,comm | awk '$2=="rpg_space.+x" && $1 !~ /Z/{f=1} END{exit !f}' || { setsid nohup "$APP/ops/+x/rpg_space.+x" "$HOUSE" "$APP" >/dev/null 2>&1 < /dev/null & }
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"
sleep 8; printf 'CONFIRM_START_MAP:rpg-space\n' >> "$INBOX"
