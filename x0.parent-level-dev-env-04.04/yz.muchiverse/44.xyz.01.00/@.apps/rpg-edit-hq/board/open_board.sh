#!/bin/sh
# Own board for rpg-edit-hq. Does not open or replace the piececraft-hq board.
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; HOUSE="$(cd "$APP/../.." && pwd)"
export PCHQ_PKG="$APP/rpg-edit-hq-pchq"
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
# the town writer paints maps/edit and then keeps the eight sprites walking
ps -eo stat,comm | awk '$2=="rpg_edit.+x" && $1 !~ /Z/{f=1} END{exit !f}' || { setsid nohup "$APP/ops/+x/rpg_edit.+x" "$HOUSE" "$APP" >/tmp/rpg-edit-walk.log 2>&1 < /dev/null & sleep 1; }
mkdir -p "$HERE/state"
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"
# open the map as soon as the engine has its session and a first frame, not after a blind 8 s (the window shows a "loading" panel until then)
_t=0
while [ "$_t" -lt 100 ]; do
    _cr="$(sed -n 's/^canvas_raw=//p' "$HERE/state/ui.txt" 2>/dev/null | head -1)"
    [ -n "$_cr" ] && [ -s "$_cr" ] && break
    sleep 0.25; _t=$((_t + 1))
done
printf 'CONFIRM_START_MAP:edit\nCONFIRM_SET_DESK:start\n' >> "$INBOX"
