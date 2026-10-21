#!/bin/sh
# Open the game-dev office on the shared board. The picture is maps/game-dev-hq/office.
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; HOUSE="$(cd "$APP/../.." && pwd)"
export PCHQ_PKG="$APP/game-dev-hq-pchq"
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
[ -x "$APP/ops/+x/game_dev_hq.+x" ] || sh "$APP/ops/build_game_dev_hq.sh"
mkdir -p "$HERE/state"
"$APP/ops/+x/game_dev_hq.+x" exportbook >/dev/null
# One studio daemon writes ../state/ui.txt. A module of this window would use board/ as its app dir.
ps -eo args | awk -v a="$APP" 'index($0,"game_dev_hq.+x") && index($0,a){f=1} END{exit !f}' || { setsid nohup "$APP/ops/+x/game_dev_hq.+x" "$HOUSE" "$APP" >/dev/null 2>&1 < /dev/null & }
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"
sleep 8; printf 'CONFIRM_START_MAP:game-dev-hq\n' >> "$INBOX"
