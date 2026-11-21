#!/bin/sh
# Toys row "Draw Battle". Opens this app's own board (PCHQ_PKG + PCHQ_BOARD_TPL).
# Usage: sh button.sh run. The taskbar already backgrounds this. open_pchq_board.sh registers the window.
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"
export PCHQ_PKG="$HERE/draw-battle-pchq"
export PCHQ_BOARD_TPL="$HERE/board/pchq-board.xhtpm"
exec sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE"
