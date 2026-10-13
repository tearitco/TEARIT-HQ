#!/bin/sh
# open_board.sh - open the cdda-hq BOARD window (cdda-hq as a module of the pc-hq board engine; the standalone window stays: sh ../button.sh run).
# 1) export the rooms as the book maps/cdda-hq (map.png, events.txt, bar.txt, ascii_legend.txt, 3D set)  2) launch the pc-hq board with THIS folder's template
# (open_pchq_board.sh takes PCHQ_BOARD_TPL; the template must be named pchq-board.xhtpm)  3) tell the board to open the cdda-hq book.
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; HOUSE="$(cd "$APP/../.." && pwd)"
# cdda-hq runs on its OWN pc-hq instance (PCHQ-SESSIONS-DESIGN.md): data folder <app>/cdda-hq-pchq (world, hero, xelector, maps, engine sessions), programs shared from @.apps/piececraft-hq. The launcher
# creates the folder when it is missing and only ever replaces THIS instance's board window, never another game's.
export PCHQ_PKG="$APP/cdda-hq-pchq"
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
mkdir -p "$HERE/state"; [ -f "$HERE/state/sides.txt" ] || printf "side_left=1\nside_right=1\n" > "$HERE/state/sides.txt"
"$APP/ops/+x/cdda_hq.+x" export3d >/dev/null 2>&1
# the ONE cdda-hq daemon (walks the pets, chat, clock, writes ../state/ui.txt): not a <module> of this window, a module runs with THIS folder as its app dir and would start a second house in board/state
ps -eo stat,comm | awk '$2=="cdda_hq.+x" && $1 !~ /Z/{f=1} END{exit !f}' || { setsid nohup "$APP/ops/+x/cdda_hq.+x" "$HOUSE" "$APP" >/dev/null 2>&1 < /dev/null & }
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"
sleep 8; printf 'CONFIRM_START_MAP:cdda-hq\n' >> "$INBOX"
