#!/bin/bash
# test-harn-same/button.sh - MINIMAL entry point for mutaclsym's own
# key-injection UX test harness (K3 style - real per-keystroke
# injection through the real running app, matching #.haiku+/tpmos-re-
# dox/_.0.aigent-testing-k3.txt). Created 2026-07-31, this project had
# no existing harness at all before this. Real logic lives in ops/
# (tk_inject_key, tk_assert_contains, tk_focus_item - byte-identical
# copies of the pal-chain family's own project-agnostic versions of
# these primitives, not reinvented), scenarios/ holds the actual test
# sequence. Unlike the pal-chain family, mutaclsym runs IN-PLACE (no
# pieces/sessions/<id>/ isolation - confirmed via button.sh's own real
# "run" action, a plain `exec system/orchestrator` from $SCRIPT_DIR).
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ACTION="${1:-help}"
OPS_X="$SCRIPT_DIR/ops/+x"

case "$ACTION" in
    compile|c|build)
        mkdir -p "$OPS_X"
        for op in tk_inject_key tk_assert_contains tk_focus_item; do
            gcc -Wall -Wextra -O2 -o "$OPS_X/$op.+x" "$SCRIPT_DIR/ops/$op.c" \
                && echo "OK   $op" || echo "FAIL $op"
        done
        ;;
    demo)
        if [ ! -x "$OPS_X/tk_inject_key.+x" ]; then
            echo "Ops not compiled yet - running compile first..."
            "$0" compile
        fi
        bash "$SCRIPT_DIR/scenarios/demo_module_split_smoke.sh"
        ;;
    kill|k|stop)
        bash "$PROJECT_DIR/button.sh" kill >/dev/null 2>&1
        sleep 1
        STRAGGLERS=$(ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input|gl_mirror|chtpm_rgb_render)|prisc\+x" | grep -v grep)
        if [ -n "$STRAGGLERS" ]; then
            echo "$STRAGGLERS" | awk '{print $2}' | xargs -r kill -9
            sleep 1
        fi
        if ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input|gl_mirror|chtpm_rgb_render)|prisc\+x" | grep -v grep >/dev/null 2>&1; then
            echo "WARNING: some processes are still running - check manually."
        else
            echo "clean"
        fi
        ;;
    help|h|-h|--help|*)
        echo "test-harn-same/button.sh - key-injected UX test harness for mutaclsym"
        echo "Usage: ./test-harn-same/button.sh <compile|demo|kill|help>"
        ;;
esac
