#!/bin/bash
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ACTION="${1:-help}"
OPS_X="$SCRIPT_DIR/ops/+x"

case "$ACTION" in
    compile|c|build)
        mkdir -p "$OPS_X"
        for op in tk_inject_key tk_type_text tk_focus_item tk_assert_contains; do
            gcc -Wall -Wextra -O2 -o "$OPS_X/$op.+x" "$SCRIPT_DIR/ops/$op.c" && echo "OK $op" || echo "FAIL $op"
        done
        ;;
    demo)
        [ -x "$OPS_X/tk_inject_key.+x" ] || "$0" compile
        bash "$SCRIPT_DIR/scenarios/demo_loader_home.sh"
        ;;
    kill|k|stop)
        bash "$PROJECT_DIR/button.sh" kill >/dev/null 2>&1
        sleep 1
        echo "clean"
        ;;
    *)
        echo "Usage: $0 compile|demo|kill"
        ;;
esac
