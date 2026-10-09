#!/bin/sh
# Toys row. Opens the chess book on the title desk.
# Do not run this while a board session is already up unless you mean
# to restart that window. open_pchq_board.sh reaps a dead board session.
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"
PCHQ="$HOUSE/@.apps/piececraft-hq"
INBOX="$PCHQ/pieces/system/widget_cmds/inbox.txt"
mkdir -p "$(dirname "$INBOX")"
sh "$PCHQ/ops/chess_event.sh" start_standard
printf 'CONFIRM_SET_DESK:title\n' > "$INBOX"
exec sh "$PCHQ/open_pchq_board.sh" "$HOUSE"
