#!/bin/bash
# button.sh - open the dsr-rpg BOARD window (a toy: @.apps/dsr-rpg-board/toy.pdl). Usage: sh button.sh run
# The toys menu lists a toy per @.apps/<dir>/toy.pdl, so this thin folder gives the board window its own entry next to "dsr-rpg" (the standalone window).
# All the files live in @.apps/dsr-rpg/board/ (template, css, side_style.pdl, open_board.sh). The board runs on its own pc-hq instance (@.apps/dsr-rpg/dsr-rpg-pchq, PCHQ-SESSIONS-DESIGN.md), so it opens
# next to the rpg-pet board instead of replacing it. Detached, so the menu returns at once (open_board.sh waits for the engine).
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
setsid nohup sh "$HERE/../dsr-rpg/board/open_board.sh" >/tmp/dsr-rpg-board.log 2>&1 < /dev/null &
disown 2>/dev/null || true
echo "dsr-rpg board launching"
