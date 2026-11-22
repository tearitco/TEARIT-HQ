#!/bin/bash
# button.sh - launcher of the Teario-Play window (see board/open_board.sh)
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
setsid nohup sh "$HERE/board/open_board.sh" >/tmp/teario-play.log 2>&1 < /dev/null &
disown 2>/dev/null || true
echo "teario-play launching"
