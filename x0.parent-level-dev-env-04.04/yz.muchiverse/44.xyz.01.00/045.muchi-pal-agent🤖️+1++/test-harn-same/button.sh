#!/bin/bash
# test-harn-same/button.sh - MINIMAL entry point for muchi-pal-agent's
# key-injection UX test harness. Unlike 044/041.pal-chain/041.pal-forum,
# this project is SINGLE-INSTANCE by design (button.sh's own "run"
# action reaps any existing session before starting a new one) - so
# this harness runs ONE session, not two concurrent ones. Real logic
# lives in ops/ (tk_inject_key, tk_type_text, tk_focus_item,
# tk_assert_contains - byte-identical copies, project-agnostic),
# scenarios/ holds the actual test sequence.
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
        bash "$SCRIPT_DIR/scenarios/demo_list_dir_tool.sh"
        ;;
    all)
        # Run every scenario in order, then print a KPI summary. Each
        # scenario emits PASS:/FAIL: lines and an OVERALL line, and exits
        # nonzero on failure - so we can just total them up here. This is
        # the "ALWAYS" regression sweep: run it any time before shipping,
        # it proves the whole UX (list_dir + tools + iqabod + remember).
        "$0" compile || { echo "compile failed"; exit 1; }
        SCENARIOS=$(ls "$SCRIPT_DIR"/scenarios/demo_*.sh | sort)
        SUM_PASS=0; SUM_FAIL=0; OVERALL=0
        tmpdir=$(mktemp -d /tmp/th_all.XXXXXX)
        for sc in $SCENARIOS; do
            name=$(basename "$sc")
            echo "--- running $name ---"
            # Run to a temp FILE, not command substitution: the scenarios
            # launch backgrounded `setsid ... & disown` app processes, and
            # under $(...) those inherit the capture pipe, so the subshell
            # hangs / the output gets lost (seen live: model_remember went
            # blank under `all` while passing standalone). A file has no
            # such fd-sharing problem.
            bash "$sc" > "$tmpdir/$name.out" 2>&1
            rc=$?
            passed=$(grep -c '^PASS:' "$tmpdir/$name.out" || true)
            failed=$(grep -c '^FAIL:' "$tmpdir/$name.out" || true)
            grep -E '^(PASS|FAIL|OVERALL):' "$tmpdir/$name.out"
            SUM_PASS=$((SUM_PASS + passed))
            SUM_FAIL=$((SUM_FAIL + failed))
            if [ "$rc" != "0" ]; then OVERALL=$((OVERALL + 1)); fi
            echo
        done
        echo "======================================"
        echo "KPI SUMMARY: pass=$SUM_PASS fail=$SUM_FAIL scenarios_failed=$OVERALL/$(( $(echo "$SCENARIOS" | wc -l) ))"
        rm -rf "$tmpdir"
        if [ "$OVERALL" != "0" ]; then echo "=== OVERALL: FAIL ==="; exit 1
        else echo "=== OVERALL: PASS ==="; fi
        ;;
    kill|k|stop)
        bash "$PROJECT_DIR/pieces/os/kill_all.sh" >/dev/null 2>&1
        sleep 1
        STRAGGLERS=$(ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|manager/\+x/path_nav_manager|prisc\+x" | grep -v grep)
        if [ -n "$STRAGGLERS" ]; then
            echo "$STRAGGLERS" | awk '{print $2}' | xargs -r kill -9
            sleep 1
        fi
        if ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|manager/\+x/path_nav_manager|prisc\+x" | grep -v grep >/dev/null 2>&1; then
            echo "WARNING: some processes are still running - check manually."
        else
            echo "clean"
        fi
        ;;
    help|h|-h|--help|*)
        echo "test-harn-same/button.sh - key-injected UX test harness for muchi-pal-agent"
        echo "Usage: ./test-harn-same/button.sh <compile|demo|all|kill|help>"
        echo "  all = regression sweep over every demo_*.sh scenario (KPI summary)"
        ;;
esac
