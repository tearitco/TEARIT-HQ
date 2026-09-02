#!/bin/bash
# drag-drop-test/button.sh - thin entry point for drag-drop test harness
# Mirrors the architecture of the actual apps: button.sh stays thin,
# ops/ contains the real logic as small reusable C binaries.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-help}"

OPS_DIR="$SCRIPT_DIR/ops"
OPS_X="$OPS_DIR/+x"

case "$ACTION" in
    compile|c|build)
        mkdir -p "$OPS_X"
        for op in dd_set_positions dd_find_window dd_move_window dd_drag_drop dd_assert_file dd_check_import; do
            gcc -Wall -Wextra -O2 -o "$OPS_X/$op.+x" "$OPS_DIR/$op.c" \
                && echo "OK   $op" || echo "FAIL $op"
        done
        ;;

    demo|test|run)
        if [ ! -x "$OPS_X/dd_set_positions.+x" ]; then
            echo "Ops not compiled yet - running compile first..."
            "$0" compile
        fi
        bash "$SCRIPT_DIR/scenarios/test_basic_import.sh"
        ;;

    kill|k|stop)
        echo "kill: stopping mutaclsym and muchi-pals..."
        # Kill mutaclsym
        if [ -f "../101.mutaclsym🧟‍♂️️+18.00/pieces/world_01/map_start/hero/window.pid" ]; then
            kill $(cat "../101.mutaclsym🧟‍♂️️+18.00/pieces/world_01/map_start/hero/window.pid") 2>/dev/null
        fi
        pkill -f "gl_mirror" 2>/dev/null
        pkill -f "mutaclsym" 2>/dev/null
        # Kill muchi-pals
        if [ -f "../01.muchi-pals-🥚️-13.01/pieces/world_01/map_lobby/egg_1/window.pid" ]; then
            kill $(cat "../01.muchi-pals-🥚️-13.01/pieces/world_01/map_lobby/egg_1/window.pid") 2>/dev/null
        fi
        pkill -f "egg_window" 2>/dev/null
        pkill -f "muchi-pals" 2>/dev/null
        sleep 1
        echo "clean"
        ;;

    help|h|-h|--help|*)
        echo "drag-drop-test/button.sh - visual drag-drop test harness"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c   - build the ops/ binaries"
        echo "  demo, test   - run the basic import test scenario"
        echo "  kill, k      - stop running test processes"
        echo "  help, h      - this text"
        echo ""
        echo "See README.txt for architecture and how to use ops directly."
        ;;
esac
