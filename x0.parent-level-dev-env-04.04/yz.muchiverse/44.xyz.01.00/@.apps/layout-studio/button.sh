#!/bin/bash
# button.sh - layout-studio toy (IN-GAME-LAYOUTS-PLAN.md). Invoked as `sh button.sh run` from the Toys menu
# (khtpm_taskbar_manager.c livedesk_build_toys_menu(): argv[1] only, house_root derived here).
# Phase 0: opens the SANDBOX pc-hq board (a copy of the board template that loads layout fragments, see
# sandbox/test-menu) next to the live one. The studio itself (layout_op + editor window) comes later.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
export DISPLAY="${DISPLAY:-:0}"
cd "$HOUSE_ROOT" || exit 1
PCHQ_BOARD_TPL="$SCRIPT_DIR/sandbox/pchq-board.xhtpm" exec sh '@.apps/piececraft-hq/open_pchq_board.sh' "$HOUSE_ROOT"
