#!/bin/bash
# button.sh - launcher of the Robot Trap Auto board window (see board/open_board.sh). Registered through the spawn funnel (AGENTS.md: every house process is a row in the process registry), same shape as teario-make/button.sh.
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"
KH_ROLE=tool KH_APP=rta-board sh "$HOUSE/&.widgits/_shared-lib/kh_spawn.sh" "$HOUSE" 0 rta-board-launch -- sh "$HERE/board/open_board.sh" >/dev/null
disown 2>/dev/null || true
echo "rta-board launching"
