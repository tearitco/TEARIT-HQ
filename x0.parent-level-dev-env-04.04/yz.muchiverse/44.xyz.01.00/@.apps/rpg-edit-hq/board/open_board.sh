#!/bin/sh
# open_board.sh - open the rpg-edit-hq BOARD window (rpg-edit-hq as a module of the pc-hq board engine; the standalone window stays: sh ../button.sh run).
# 1) export the rooms as the book maps/rpg-edit-hq (map.png, events.txt, bar.txt, ascii_legend.txt, 3D set)  2) launch the pc-hq board with THIS folder's template
# (open_pchq_board.sh takes PCHQ_BOARD_TPL; the template must be named pchq-board.xhtpm)  3) tell the board to open the rpg-edit-hq book.
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; HOUSE="$(cd "$APP/../.." && pwd)"
# rpg-edit-hq runs on its OWN pc-hq instance (PCHQ-SESSIONS-DESIGN.md): data folder <app>/rpg-edit-hq-pchq (world, hero, xelector, maps, engine sessions), programs shared from @.apps/piececraft-hq. The launcher
# creates the folder when it is missing and only ever replaces THIS instance's board window, never another game's.
export PCHQ_PKG="$APP/rpg-edit-hq-pchq"
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
mkdir -p "$HERE/state"; [ -f "$HERE/state/sides.txt" ] || printf "side_left=1\nside_right=1\n" > "$HERE/state/sides.txt"
"$APP/ops/+x/rpg_edit_hq.+x" export3d >/dev/null 2>&1
# the ONE rpg-edit-hq daemon (walks the pets, chat, clock, writes ../state/ui.txt): not a <module> of this window, a module runs with THIS folder as its app dir and would start a second house in board/state
ps -eo stat,comm | awk '$2=="rpg_edit_hq.+x" && $1 !~ /Z/{f=1} END{exit !f}' || { setsid nohup "$APP/ops/+x/rpg_edit_hq.+x" "$HOUSE" "$APP" >/dev/null 2>&1 < /dev/null & }
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"
# Last edited book, or the Islands demo (page start) when the row is missing or names a book this instance cannot open.
LINE=$(sh "$APP/ops/edit_last.sh" resolve "$APP/state/last_book.txt" --house "$HOUSE" --maps "$PCHQ_PKG/pieces/system/maps" --demo-book rpg-edit-hq --demo-page start) || exit 1
BOOK=${LINE#book=}; BOOK=${BOOK%% page=*}; PAGE=${LINE#*page=}
case "$BOOK" in
    *[!A-Za-z0-9_-]*|"") exit 1 ;;
esac
# The edit package only ships its own map. Every other book with a game.pdl is a link, so the book dropdown can list it. A new book appears without a code change.
for src in "$HOUSE"/@.apps/*/*-pchq/pieces/system/maps/*/game.pdl; do
    [ -f "$src" ] || continue
    bookdir=$(dirname "$src")
    bookname=$(basename "$bookdir")
    link="$PCHQ_PKG/pieces/system/maps/$bookname"
    [ -e "$link" ] || [ -L "$link" ] && continue
    ln -s "$bookdir" "$link"
done
LINK="$PCHQ_PKG/pieces/system/maps/$BOOK"
SRC="$HOUSE/@.apps/$BOOK/$BOOK-pchq/pieces/system/maps/$BOOK"
if [ ! -d "$LINK" ] && [ -d "$SRC" ]; then
    ln -s "$SRC" "$LINK"
fi
if [ ! -d "$PCHQ_PKG/pieces/system/maps/$BOOK" ]; then
    BOOK=rpg-edit-hq
    PAGE=start
fi
sleep 8; printf 'CONFIRM_START_MAP:%s\nCONFIRM_SET_DESK:%s\n' "$BOOK" "$PAGE" >> "$INBOX"
