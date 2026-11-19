#!/bin/bash
# button.sh - launcher of the Teario-Make window (its OWN board window, owner 2026-11-19; plan BOOK 16.game TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN, phase P1). Not the rpg-edit editor.
# Usage: sh button.sh run     (a relaunch starts a game driver, an engine session and a window: it asks cpu_gate first, see board/open_board.sh)
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
setsid nohup sh "$HERE/board/open_board.sh" >/tmp/teario-make.log 2>&1 < /dev/null &
disown 2>/dev/null || true
echo "teario-make launching"
