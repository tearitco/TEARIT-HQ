#!/bin/sh
# Toys row. Opens piececraft on the Doom book (title desk) the same
# way a New Game event does, then the board window.
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"
PCHQ="$HOUSE/@.apps/piececraft-hq"
WORLD="$PCHQ/pieces/world_01/state.txt"
INBOX="$PCHQ/pieces/system/widget_cmds/inbox.txt"
mkdir -p "$(dirname "$INBOX")"
if [ -f "$WORLD" ]; then
    grep -q '^map_id=' "$WORLD" && sed -i 's/^map_id=.*/map_id=doom/' "$WORLD" || printf 'map_id=doom\n' >> "$WORLD"
    grep -q '^desk_id=' "$WORLD" && sed -i 's/^desk_id=.*/desk_id=title/' "$WORLD" || printf 'desk_id=title\n' >> "$WORLD"
fi
sh "$PCHQ/ops/doom_event.sh" start_game doom title
# start_game writes CONFIRM_START_MAP, which loads the book's first
# desk. The title page is a named desk, so replace that one inbox line.
printf 'CONFIRM_SET_DESK:title\n' > "$INBOX"
exec sh "$PCHQ/open_pchq_board.sh" "$HOUSE"
