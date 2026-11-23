#!/bin/bash
# button.sh - launcher of the Teario-Make window (its OWN board window, owner 2026-11-19; plan BOOK 16.game TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN, phase P1). Not the rpg-edit editor.
# Usage: sh button.sh run     (a relaunch starts a game driver, an engine session and a window: it asks cpu_gate first, see board/open_board.sh)
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"   # registered through the spawn funnel (AGENTS.md: every house process is a row in the process registry); keep = the launcher outlives this shell, open_board.sh registers the real daemons itself
KH_ROLE=tool KH_APP=teario-make sh "$HOUSE/&.widgits/_shared-lib/kh_spawn.sh" "$HOUSE" 0 teario-make-launch -- sh "$HERE/board/open_board.sh" >/dev/null
disown 2>/dev/null || true
echo "teario-make launching"
