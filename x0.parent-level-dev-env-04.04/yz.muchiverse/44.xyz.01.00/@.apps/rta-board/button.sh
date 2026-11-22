#!/bin/bash
# button.sh - launcher of the Robot Trap Auto board window (see board/open_board.sh)
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
setsid nohup sh "$HERE/board/open_board.sh" >/tmp/rta-board.log 2>&1 < /dev/null &
disown 2>/dev/null || true
echo "rta-board launching"
