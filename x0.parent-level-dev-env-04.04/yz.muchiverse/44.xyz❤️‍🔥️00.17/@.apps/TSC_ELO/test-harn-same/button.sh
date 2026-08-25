#!/bin/bash
# test-harn-same/button.sh - MINIMAL entry point for the TSC_ELO PvP
# harness. Mirrors the 044.pal-chat-irc👥️+2/test-harn-same/button.sh
# convention: compile/run/kill actions, thin button.sh, real logic in
# ops/ (small reusable C binaries) + scenarios/. The interesting logic
# is NOT here.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ACTION="${1:-help}"

OPS_DIR="$SCRIPT_DIR/ops"
OPS_X="$OPS_DIR/+x"

case "$ACTION" in
    compile|c|build)
        mkdir -p "$OPS_X"
        for op in tk_inject_key tk_type_text tk_focus_item tk_assert_contains \
                  tsc_cmd tsc_answer; do
            gcc -Wall -Wextra -O2 -o "$OPS_X/$op.+x" "$OPS_DIR/$op.c" \
                && echo "OK   $op" || echo "FAIL $op"
        done
        ;;

    pvp|run)
        if [ ! -x "$OPS_X/tk_inject_key.+x" ]; then
            echo "Ops not compiled yet - running compile first..."
            "$0" compile
        fi
        bash "$SCRIPT_DIR/scenarios/pvp_duel.sh"
        ;;

    check|verify)
        BAD=0
        for op in tk_inject_key tk_type_text tk_focus_item tk_assert_contains \
                  tsc_cmd tsc_answer; do
            if [ -x "$OPS_X/$op.+x" ]; then echo "OK   ops/$op.+x"; else echo "MISSING ops/$op.+x"; BAD=1; fi
        done
        for b in ops/+x/tsc_net.+x ops/+x/tsc_setup.+x ops/+x/palnet_peer.+x; do
            if [ -x "$PROJECT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; BAD=1; fi
        done
        [ "$BAD" = "0" ] && echo "check: all harness binaries present" || echo "check: run 'compile' first"
        ;;

    kill|k|stop)
        # Defense-in-depth: project kill + explicit net-process sweep.
        bash "$PROJECT_DIR/button.sh" kill >/dev/null 2>&1
        pkill -f "ops/+x/palnet_peer" 2>/dev/null
        pkill -f "ops/+x/tsc_net" 2>/dev/null
        sleep 1
        STRAGGLERS=$(ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|ops/\+x/(palnet_peer|tsc_net)|prisc\+x" | grep -v grep)
        if [ -n "$STRAGGLERS" ]; then
            echo "$STRAGGLERS" | awk '{print $2}' | xargs -r kill -9
            sleep 1
        fi
        if ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|ops/\+x/(palnet_peer|tsc_net)|prisc\+x" | grep -v grep >/dev/null 2>&1; then
            echo "WARNING: some processes are still running - check manually."
        else
            echo "clean"
        fi
        ;;

    help|h|-h|--help|*)
        echo "test-harn-same/button.sh - TSC_ELO PvP-over-P2P harness"
        echo ""
        echo "Usage: ./test-harn-same/button.sh <action>"
        echo "  compile, c   - build the ops/ binaries"
        echo "  check, v     - verify all harness + peer binaries exist"
        echo "  pvp, run     - run the 2-subharness PvP duel scenario"
        echo "  kill, k      - clean up any running TSC_ELO/net processes"
        echo "  help, h      - this text"
        ;;
esac
