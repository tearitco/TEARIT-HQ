#!/bin/bash
# button.sh - launcher of the Voxel window (D112, see board/open_board.sh). Opens a real window: start it only through cpu_gate, never while another board window is being driven.
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
setsid nohup sh "$HERE/board/open_board.sh" >/tmp/vox.log 2>&1 < /dev/null &
disown 2>/dev/null || true
echo "vox launching"
