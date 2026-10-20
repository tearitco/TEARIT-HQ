#!/bin/bash
# Open the game-dev-hq window. Usage: sh button.sh run
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
XHTPM="$SCRIPT_DIR/game-dev-hq.xhtpm"
[ "${1:-run}" = run ] || exit 0
mkdir -p "$SCRIPT_DIR/state"
# The painted office has no camera. The board window is the same studio on the engine frame (0 = 3D, 1-4 = POV).
old="$(ps -eo pid,comm,args | awk -v x="$XHTPM" '$2=="khtpm_core_rend" && index($0,x){print $1}')"
[ -n "$old" ] && echo "$old" | xargs -r kill -TERM && sleep 1
exec sh "$SCRIPT_DIR/board/open_board.sh"
