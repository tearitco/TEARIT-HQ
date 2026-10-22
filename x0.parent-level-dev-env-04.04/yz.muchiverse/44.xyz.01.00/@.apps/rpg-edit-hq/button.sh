#!/bin/bash
# button.sh - 12.toys row "rpg-edit-hq". Own board, map book edit / page start.
# Does not open the piececraft-hq board. Usage: sh button.sh run
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
setsid nohup sh "$HERE/board/open_board.sh" >/tmp/rpg-edit-hq.log 2>&1 < /dev/null &
disown 2>/dev/null || true
echo "edit launching"
