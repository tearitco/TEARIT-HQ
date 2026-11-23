#!/bin/bash
# button.sh - launcher of the Teario-Play window (see board/open_board.sh)
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"   # registered through the spawn funnel (AGENTS.md: every house process is a row in the process registry); keep = the launcher outlives this shell, open_board.sh registers the real daemons itself
KH_ROLE=tool KH_APP=teario-play sh "$HOUSE/&.widgits/_shared-lib/kh_spawn.sh" "$HOUSE" 0 teario-play-launch -- sh "$HERE/board/open_board.sh" >/dev/null
disown 2>/dev/null || true
echo "teario-play launching"
