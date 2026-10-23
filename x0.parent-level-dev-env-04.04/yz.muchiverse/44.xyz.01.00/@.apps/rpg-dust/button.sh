#!/bin/bash
# button.sh - open the rpg-dust BOARD window (a toy: @.apps/rpg-dust/toy.pdl; the toys menu lists a toy per @.apps/<dir>/toy.pdl). Usage: sh button.sh run
# The board is board/open_board.sh: it exports the zoom ledger's current address (region page + the zoomed area page) into its own pc-hq instance @.apps/rpg-dust/rpg-dust-pchq
# and opens the shared pc-hq board with this app's template copy. Detached so the menu returns at once (open_board.sh waits for the engine). Log: /tmp/rpg-dust-board.log.
# Close the board from the taskbar. First start builds the ops (ops/build_dust_check.sh) when they are missing.
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
setsid nohup sh "$HERE/board/open_board.sh" >/tmp/rpg-dust-board.log 2>&1 < /dev/null &
disown 2>/dev/null || true
echo "rpg-dust board launching"
