#!/bin/bash
# button.sh - open the rpg-pet BOARD window (a toy: @.apps/rpg-pet-board/toy.pdl). Usage: sh button.sh run
# The toys menu lists a toy per @.apps/<dir>/toy.pdl, so this thin folder gives the board window its own entry next to "rpg-pet" (the standalone window).
# All the files live in @.apps/rpg-pet/board/ (template, css, side_style.pdl, open_board.sh). It uses the pc-hq board launcher, which replaces any open pc-hq board window
# (one board window at a time: both match the same single-instance pattern). Detached, so the menu returns at once (open_board.sh waits for the engine).
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
setsid nohup sh "$HERE/../rpg-pet/board/open_board.sh" >/tmp/rpg-pet-board.log 2>&1 < /dev/null &
disown 2>/dev/null || true
echo "rpg-pet board launching"
