#!/bin/bash
# test-harn/button.sh - MINIMAL entry point for the cross-project
# egg_window -> gl_mirror integration test (0.a-z-pets-plan/a-z-fix.txt).
# This is a genuinely cross-project test (egg_window lives in
# 01.muchi-pals, gl_mirror lives in 101.mutaclsym), so it lives here in
# the neutral plan directory rather than nested inside just one of them.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OPS_X="$SCRIPT_DIR/ops/+x"
ACTION="${1:-help}"

case "$ACTION" in
    compile|c|build)
        mkdir -p "$OPS_X"
        gcc -std=c11 -Wall -Wextra -O2 -o "$OPS_X/tk_drag_sim.+x" "$SCRIPT_DIR/ops/tk_drag_sim.c" -lX11 \
            && echo "OK   tk_drag_sim" || echo "FAIL tk_drag_sim"
        gcc -std=c11 -Wall -Wextra -O2 -o "$OPS_X/tk_screenshot.+x" "$SCRIPT_DIR/ops/tk_screenshot.c" -lX11 \
            && echo "OK   tk_screenshot" || echo "FAIL tk_screenshot"
        ;;
    demo)
        if [ ! -x "$OPS_X/tk_drag_sim.+x" ]; then
            echo "Ops not compiled yet - running compile first..."
            "$0" compile
        fi
        bash "$SCRIPT_DIR/scenarios/demo_egg_to_mirror.sh"
        ;;
    kill|k|stop)
        pkill -f "system/egg_window" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        sleep 1
        if ps aux | grep -E "system/egg_window|system/gl_mirror" | grep -v grep >/dev/null 2>&1; then
            echo "WARNING: some processes are still running - check manually."
        else
            echo "clean"
        fi
        ;;
    help|h|-h|--help|*)
        echo "test-harn/button.sh - egg_window -> gl_mirror integration test"
        echo "Usage: ./test-harn/button.sh <compile|demo|kill|help>"
        ;;
esac
