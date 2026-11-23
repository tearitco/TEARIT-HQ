#!/bin/bash
# button.sh - launcher of the Voxel window (D112, see board/open_board.sh). Opens a real window: start it only through cpu_gate, never while another board window is being driven. Registered through the spawn funnel (AGENTS.md: every house process is a row in the process registry), same shape as teario-make/button.sh.
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"
KH_ROLE=tool KH_APP=vox sh "$HOUSE/&.widgits/_shared-lib/kh_spawn.sh" "$HOUSE" 0 vox-launch -- sh "$HERE/board/open_board.sh" >/dev/null
disown 2>/dev/null || true
echo "vox launching"
