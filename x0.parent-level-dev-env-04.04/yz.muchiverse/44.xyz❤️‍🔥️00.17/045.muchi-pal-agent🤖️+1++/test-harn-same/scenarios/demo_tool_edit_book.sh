#!/bin/bash
# demo_tool_edit_book.sh - H2 of 5.tool-scaffold-gemma-agentic:
# "dev does basic agentic coding with gemma" scenario #2 - create a book
# file, append a line, read it back, search it. Real keystrokes through
# the deterministic pipeline. Proves write_file, edit_file (append
# sub-mode), read_file, and search_in_files dispatch in one turn-per-tool
# agentic session - the exact loop a human dev drives.
#
# KPIs (design doc section 8):
#   K5: book.txt content matches the 2 lines exactly (create + append landed)
#   K6: read result shows the full file; search result shows book.txt [Line 2]
#   K7: four strategy_log entries, strategy=A tool=<right tool>, in order
#   K8: frame shows all four results after their messages; no 'TOOL:' garbage
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

echo "=== H2: edit a book (create + append + read + search) ==="
bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
sleep 1
rm -rf "$PROJECT_DIR/pieces/sessions"/*
NO_GL=1 setsid bash button.sh run --pal > /tmp/th_h2.log 2>&1 < /dev/null & disown

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
    fail "session launch - current_frame.txt never appeared within 30s (check /tmp/th_h2.log)"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
STRATEGY_LOG="$SESS/pieces/world_01/session_01/chat/strategy_log.txt"
echo "Session: $SESS"
BEFORE_LOG_LINES=$(wc -l < "$STRATEGY_LOG" 2>/dev/null || echo 0)

do_turn() {
    echo "--- keystroke: $1 ---"
    key "$SESS" 13
    type_ "$SESS" "$1"
    key "$SESS" 13
    sleep 1.5
}

do_turn "create file book.txt containing Chapter 1: Hello"
do_turn 'append to book.txt the line "It was a dark and stormy night."'
do_turn "read file book.txt"
do_turn "search for dark in book.txt"
sleep 1
cp "$FRAME" "$PROOF_DIR/after_all.txt"
cp "$STRATEGY_LOG" "$PROOF_DIR/strategy_log.txt" 2>/dev/null

echo "--- NEW strategy_log.txt lines from this run only ---"
NEW_LOG_LINES=$(tail -n "+$((BEFORE_LOG_LINES + 1))" "$STRATEGY_LOG" 2>/dev/null)
echo "$NEW_LOG_LINES"

# K5 - create + append both landed, exactly 2 lines, exact content
EXPECTED="Chapter 1: Hello
It was a dark and stormy night."
if [ -f "$SESS/book.txt" ] && [ "$(cat "$SESS/book.txt")" = "$EXPECTED" ]; then
    pass "K5: book.txt content matches the 2 lines exactly"
else
    fail "K5: book.txt missing or wrong content (got: '$(cat "$SESS/book.txt" 2>/dev/null)')"
fi

# K6 - read result shows the full file; search result shows book.txt [Line 2]
if grep -A2 "\[read_file result\]:" "$FRAME" | grep -q "Chapter 1: Hello" && \
   grep -A2 "\[read_file result\]:" "$FRAME" | grep -q "It was a dark and stormy night."; then
    pass "K6a: read_file result shows the full 2-line file"
else
    fail "K6a: read_file result does not show the full file"
fi
if grep -q "\[search_in_files result\]:" "$FRAME" && \
   grep "\[search_in_files result\]:" "$FRAME" | grep -q "book.txt \[Line 2\]"; then
    pass "K6b: search result shows 'book.txt [Line 2]'"
else
    fail "K6b: search result missing 'book.txt [Line 2]'"
fi

# K7 - four strategy_log entries, in order
ORDER_OK=1
LAST_LN=0
for TOOL in write_file edit_file read_file search_in_files; do
    LN=$(echo "$NEW_LOG_LINES" | grep -n "tool=$TOOL" | head -1 | cut -d: -f1)
    if [ -z "$LN" ]; then
        echo "  FAIL: no tool=$TOOL entry"; ORDER_OK=0
    elif [ "$LN" -gt "$LAST_LN" ]; then
        echo "  ok: tool=$TOOL at log line $LN"; LAST_LN=$LN
    else
        echo "  FAIL: tool=$TOOL not in order (line $LN <= $LAST_LN)"; ORDER_OK=0
    fi
done
[ "$ORDER_OK" = "1" ] && pass "K7: four strategy_log entries in order (write, append, read, search)" || fail "K7: strategy_log order"

# K8 - all four results render after their messages, no 'TOOL:' garbage
echo "--- K8: results below their user lines + no 'TOOL:' garbage ---"
OK=1
for PAIR in "create file book.txt containing Chapter 1: Hello|write_file result" \
            'append to book.txt the line "It was a dark and stormy night."|edit_file result' \
            "read file book.txt|read_file result" \
            "search for dark in book.txt|search_in_files result"; do
    USER_MSG="${PAIR%%|*}"
    RES_PAT="${PAIR#*|}"
    UL=$(grep -n "You: ${USER_MSG}" "$FRAME" | tail -1 | cut -d: -f1)
    RL=$(grep -n "$RES_PAT" "$FRAME" | tail -1 | cut -d: -f1)
    if [ -n "$UL" ] && [ -n "$RL" ] && [ "$RL" -gt "$UL" ]; then
        echo "  ok: '$RES_PAT' (line $RL) after 'You: $USER_MSG' (line $UL)"
    else
        echo "  FAIL: '$RES_PAT' user='$UL' result='$RL'"
        OK=0
    fi
done
if grep -qE "TOOL: (read|write|list|run|search|speak)" "$FRAME"; then
    echo "  FAIL: gemma 'TOOL:' hallucination in the frame"; OK=0
else
    echo "  ok: no 'TOOL:' hallucination in the frame"
fi
[ "$OK" = "1" ] && pass "K8: all four results after their messages, no 'TOOL:' garbage" || fail "K8: frame display"

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
