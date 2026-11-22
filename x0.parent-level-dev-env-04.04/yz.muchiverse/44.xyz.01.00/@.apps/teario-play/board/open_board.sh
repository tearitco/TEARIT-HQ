#!/bin/sh
# open_board.sh - the Teario-Play window: the SAME launcher as teario-make/board/open_board.sh, pointed at this app's board folder (own template, own window state, own pc-hq package, own game driver).
TEARIO_BOARD_DIR="$(cd "$(dirname "$0")" && pwd)"; export TEARIO_BOARD_DIR
exec sh "$TEARIO_BOARD_DIR/../../teario-make/board/open_board.sh"
