#!/bin/bash
# test-harn-same/button.sh - MINIMAL entry point. Mirrors the parent
# project's own button.sh conventions (compile/run/kill actions), but
# for TESTING: it launches real sessions of the actual app and drives
# them through real key injection. The interesting logic - inject a
# key, type text, focus a numbered item, assert a frame contains
# something - is NOT in here. It lives in ops/ as small, reusable,
# independently-callable C binaries (tk_inject_key, tk_type_text,
# tk_focus_item, tk_assert_contains), same architecture as the app
# itself (orchestrator/button.sh stays thin, ops/ does the real work) -
# see README.txt for the full explanation and how to compose these into
# your own scenario instead of just running "demo".
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ACTION="${1:-help}"

OPS_DIR="$SCRIPT_DIR/ops"
OPS_X="$OPS_DIR/+x"

case "$ACTION" in
    compile|c|build)
        mkdir -p "$OPS_X"
        for op in tk_inject_key tk_type_text tk_focus_item tk_assert_contains; do
            gcc -Wall -Wextra -O2 -o "$OPS_X/$op.+x" "$OPS_DIR/$op.c" \
                && echo "OK   $op" || echo "FAIL $op"
        done
        ;;

    demo)
        # The reference scenario: 2 real users, real signup/login/room-
        # join/typed-message flow, verifying the OTHER (idle) session
        # updates live with no action taken on its side. This is just
        # ONE example scenario built from the ops below - copy this
        # action as a starting point for your own scenario, don't treat
        # it as the only thing this harness can do.
        if [ ! -x "$OPS_X/tk_inject_key.+x" ]; then
            echo "Ops not compiled yet - running compile first..."
            "$0" compile
        fi
        bash "$SCRIPT_DIR/scenarios/demo_2user_chat.sh"
        ;;

    kill|k|stop)
        # Same defense-in-depth pattern as the parent project's own
        # button.sh kill - see #.haiku+/!.xyzos-pitfalls+1.txt PITFALL
        # 20/21/22 for why a single call to kill_all.sh is not trusted
        # on its own in this project.
        bash "$PROJECT_DIR/pieces/os/kill_all.sh" >/dev/null 2>&1
        sleep 1
        STRAGGLERS=$(ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|ops/\+x/(palnet_peer|chat_inbox_watcher)|prisc\+x" | grep -v grep)
        if [ -n "$STRAGGLERS" ]; then
            echo "$STRAGGLERS" | awk '{print $2}' | xargs -r kill -9
            sleep 1
        fi
        if ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|ops/\+x/(palnet_peer|chat_inbox_watcher)|prisc\+x" | grep -v grep >/dev/null 2>&1; then
            echo "WARNING: some processes are still running - check manually."
        else
            echo "clean"
        fi
        ;;

    help|h|-h|--help|*)
        echo "test-harn-same/button.sh - key-injected UX test harness for pal-chat-irc"
        echo ""
        echo "Usage: ./test-harn-same/button.sh <action>"
        echo "  compile, c   - build the ops/ binaries"
        echo "  demo         - run the reference 2-user real-UX chat scenario"
        echo "  kill, k      - clean up any running app processes (verified, not trusted)"
        echo "  help, h      - this text"
        echo ""
        echo "See test-harn-same/README.txt for the architecture and how to build"
        echo "your own scenario from the ops/ primitives, and"
        echo "#.haiku+/!.local-ux-testing-ai.txt for the underlying key-injection"
        echo "interaction model this whole harness encodes."
        ;;
esac
