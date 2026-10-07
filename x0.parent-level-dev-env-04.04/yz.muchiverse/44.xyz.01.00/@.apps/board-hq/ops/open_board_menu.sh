#!/bin/sh
# open_board_menu.sh <name> - what the tb h-ai menu rows run: open (or focus-by-reuse) the board window <name> (quests, ghosts, phones, ...).
# Finds the house from its own location, so the pdl row does not depend on the caller's working directory. A board that is already open is left alone
# (open_board.sh keeps one window per board). Close with: sh open_board.sh <house> <name> close.
HERE="$(cd "$(dirname "$0")" && pwd)"
exec sh "$HERE/open_board.sh" "$(cd "$HERE/../../.." && pwd)" "${1:?usage: open_board_menu.sh <board name>}"
