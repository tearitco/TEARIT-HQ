#!/bin/bash
# test-harn-same/button.sh - MINIMAL entry point for my-chara-txt's
# key-injection regression harness. Modeled directly on
# 045.muchi-pal-agent🤖️+1/test-harn-same/button.sh (same shape: real
# logic lives in ops/ - tk_inject_key, tk_type_text, tk_focus_item,
# tk_assert_contains, byte-identical copies, project-agnostic -
# scenarios/ holds the actual test sequences).
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
        bash "$SCRIPT_DIR/scenarios/demo_end_turn.sh"
        ;;
    all)
        "$0" compile || { echo "compile failed"; exit 1; }
        SCENARIOS=$(ls "$SCRIPT_DIR"/scenarios/demo_*.sh 2>/dev/null | sort)
        SUM_PASS=0; SUM_FAIL=0; OVERALL=0
        for s in $SCENARIOS; do
            echo ""
            echo "########## $(basename "$s") ##########"
            bash "$s"
            rc=$?
            if [ $rc -eq 0 ]; then SUM_PASS=$((SUM_PASS+1)); else SUM_FAIL=$((SUM_FAIL+1)); OVERALL=1; fi
        done
        echo ""
        echo "=== KPI SUMMARY: $SUM_PASS scenario(s) passed, $SUM_FAIL failed ==="
        exit $OVERALL
        ;;
    kill)
        (cd "$PROJECT_DIR" && bash button.sh kill 2>/dev/null)
        ;;
    help|h|-h|--help)
        echo "my-chara-txt test-harn-same/button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build the 4 reusable tk_* test ops"
        echo "  demo                - Run the default scenario (demo_end_turn.sh)"
        echo "  all                 - Run every scenarios/demo_*.sh, print KPI summary"
        echo "  kill                - Kill any lingering my-chara-txt processes"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
