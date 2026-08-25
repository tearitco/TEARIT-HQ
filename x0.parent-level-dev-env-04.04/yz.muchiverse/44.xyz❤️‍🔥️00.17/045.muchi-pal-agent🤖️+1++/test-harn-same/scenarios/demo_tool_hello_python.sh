#!/bin/bash
# demo_tool_hello_python.sh - H1 of 5.tool-scaffold-gemma-agentic:
# "dev does basic agentic coding with gemma" scenario #1 - write a python
# file, then run it. Real keystrokes through the deterministic pipeline
# (gemma_strategy -> strategy_execute_a -> send_message), same shape as
# the proven demo_list_dir_tool.sh. Proves write_file + exec_cmd dispatch
# (both were detected by gemma_strategy but write_file was NEVER executed
# before this pass - see #.haiku+/30.jul-30-handoff/5.tool-scaffold-gemma-agentic.md).
#
# KPIs (design doc section 8):
#   K1: $SESS/hello.py exists and its content == print('hello world')
#   K2: strategy_log shows strategy=A tool=write_file then tool=exec_cmd
#   K3: frame shows 'hello world' (the run output) with NO 'TOOL:' garbage
#   K4: results render after their triggering "You:" lines
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
OPS="$HARNESS_DIR/ops/+x"

PROOF_DIR="$PROJECT_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

cleanup() {
    echo; echo "--- cleanup ---"
    bash "$HARNESS_DIR/button.sh" kill
}
trap cleanup EXIT

key()   { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.2; }
type_() { "$OPS/tk_type_text.+x" "$1" "$2"; sleep 0.2; }

echo "=== H1: python hello-world (write_file + exec_cmd) ==="
bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
sleep 1
rm -rf "$PROJECT_DIR/pieces/sessions"/*
NO_GL=1 setsid bash button.sh run --pal > /tmp/th_h1.log 2>&1 < /dev/null & disown

SESS=""
for i in $(seq 1 30); do
    CANDIDATE=$(ls -dt pieces/sessions/*/ 2>/dev/null | head -1)
    if [ -n "$CANDIDATE" ] && [ -f "${CANDIDATE}pieces/display/current_frame.txt" ]; then
        SESS="${CANDIDATE%/}"
        break
    fi
    sleep 1
done

if [ -z "$SESS" ]; then
    fail "session launch - current_frame.txt never appeared within 30s (check /tmp/th_h1.log)"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
STRATEGY_LOG="$SESS/pieces/world_01/session_01/chat/strategy_log.txt"
echo "Session: $SESS"
BEFORE_LOG_LINES=$(wc -l < "$STRATEGY_LOG" 2>/dev/null || echo 0)

echo "--- keystroke 1: create file hello.py containing print('hello world') ---"
key "$SESS" 13
type_ "$SESS" "create file hello.py containing print('hello world')"
key "$SESS" 13
sleep 1.5
cp "$FRAME" "$PROOF_DIR/after_write.txt"

echo "--- keystroke 2: run python3 hello.py ---"
key "$SESS" 13
type_ "$SESS" "run python3 hello.py"
key "$SESS" 13
sleep 1.5
cp "$FRAME" "$PROOF_DIR/after_run.txt"
cp "$STRATEGY_LOG" "$PROOF_DIR/strategy_log.txt" 2>/dev/null

echo "--- NEW strategy_log.txt lines from this run only ---"
NEW_LOG_LINES=$(tail -n "+$((BEFORE_LOG_LINES + 1))" "$STRATEGY_LOG" 2>/dev/null)
echo "$NEW_LOG_LINES"

# K1
if [ -f "$SESS/hello.py" ] && [ "$(cat "$SESS/hello.py")" = "print('hello world')" ]; then
    pass "K1: hello.py exists with content exactly print('hello world')"
else
    fail "K1: hello.py missing or wrong content ($(cat "$SESS/hello.py" 2>/dev/null))"
fi

# K2
if echo "$NEW_LOG_LINES" | grep -q "tool=write_file" && \
   echo "$NEW_LOG_LINES" | grep -q "tool=exec_cmd"; then
    pass "K2: strategy_log shows strategy=A tool=write_file and tool=exec_cmd"
else
    fail "K2: missing write_file/exec_cmd strategy_log entries"
fi

# K3
if grep -q "hello world" "$FRAME"; then
    pass "K3: frame shows the run output 'hello world'"
else
    fail "K3: 'hello world' not visible in the frame"
fi
if grep -qE "TOOL: (read|write|list|run|search|speak)" "$FRAME"; then
    fail "K3b: gemma 'TOOL:' hallucination in the frame"
else
    pass "K3b: no 'TOOL:' hallucination in the frame"
fi

# K4 - both results render below their triggering user messages
echo "--- K4: tool results render below their triggering user lines ---"
OK=1
for PAIR in "create file hello.py containing print('hello world')|write_file result" \
            "run python3 hello.py|exec_cmd result"; do
    USER_MSG="${PAIR%%|*}"
    RES_PAT="${PAIR#*|}"
    UL=$(grep -n "You: ${USER_MSG}" "$FRAME" | tail -1 | cut -d: -f1)
    RL=$(grep -n "$RES_PAT" "$FRAME" | tail -1 | cut -d: -f1)
    if [ -n "$UL" ] && [ -n "$RL" ] && [ "$RL" -gt "$UL" ]; then
        echo "  ok: '$RES_PAT' (line $RL) renders after 'You: $USER_MSG' (line $UL)"
    else
        echo "  FAIL: '$RES_PAT' user='$UL' result='$RL'"
        OK=0
    fi
done
[ "$OK" = "1" ] && pass "K4: write_file and exec_cmd results render after their user messages" || fail "K4: result ordering"

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
