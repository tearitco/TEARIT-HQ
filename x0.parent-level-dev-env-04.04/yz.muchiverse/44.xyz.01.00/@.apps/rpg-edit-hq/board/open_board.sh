#!/bin/sh
# Own board for rpg-edit-hq. Does not open or replace the piececraft-hq board.
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; HOUSE="$(cd "$APP/../.." && pwd)"
export PCHQ_PKG="$APP/rpg-edit-hq-pchq"
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
mkdir -p "$PCHQ_PKG/pieces/system/maps"
rm -rf "$PCHQ_PKG/pieces/system/maps/edit"
cp -a "$HERE/maps/edit" "$PCHQ_PKG/pieces/system/maps/edit"
mkdir -p "$HERE/state"
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"
sleep 8
printf 'CONFIRM_START_MAP:edit\nCONFIRM_SET_DESK:start\n' >> "$INBOX"
