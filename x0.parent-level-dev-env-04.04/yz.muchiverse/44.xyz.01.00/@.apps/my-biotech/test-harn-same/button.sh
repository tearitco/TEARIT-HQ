#!/bin/bash
# test-harn-same/button.sh - MINIMAL entry point for my-biotech's
# key-injection regression harness. Same shape as the sibling harnesses
# in my-chara-txt and myne-qrypto/qtc.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ACTION="${1:-help}"
OPS_X="$SCRIPT_DIR/ops/+x"

case "$ACTION" in
    compile|c|build)
        mkdir -p "$OPS_X"
        for op in tk_inject_key tk_type_text tk_focus_item tk_assert_contains; do
            gcc -Wall -Wextra -O2 -o "$OPS_X/$op.+x" "$SCRIPT_DIR/ops/$op.c" \
                && echo "OK   $op" || echo "FAIL $op"
        done
        ;;
    demo)
        if [ ! -x "$OPS_X/tk_inject_key.+x" ]; then
            echo "Ops not compiled yet - running compile first..."
            "$0" compile
        fi
        bash "$SCRIPT_DIR/scenarios/demo_research_and_end_turn.sh"
        ;;
    kill)
        (cd "$PROJECT_DIR" && bash button.sh kill 2>/dev/null)
        ;;
    help|h|-h|--help)
        echo "my-biotech test-harn-same/button.sh"
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build the 4 reusable tk_* test ops"
        echo "  demo                - Run demo_research_and_end_turn.sh (SLOW - real LLM call, up to ~2min)"
        echo "  kill                - Kill any lingering my-biotech processes"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
