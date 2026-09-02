#!/bin/bash
# demo_list_dir_tool.sh - reference SCENARIO for muchi-pal-agent, built
# from test-harn-same/ops/ (same generic tk_* primitives the other
# harnesses in this family use).
#
# Tests the REAL "pre-Gemma tool call" pipeline (gemma_strategy.c +
# strategy_execute_a.c - see jul-21-gemma-fix.txt's own handoff notes
# and !.GRAND-PLAN-TOOL-SCAFFOLD.txt for the design history): Gemma is
# too small to reliably follow a TOOL: format, so tool detection is
# fully DETERMINISTIC, keyword-based, pre-decided BEFORE any LLM call -
# see gemma_strategy.c's own detect_tool(): the words "list"/"dir"/
# "files"/"show" in the user's message map to detected_tool=list_dir,
# selected_strategy=A (always pre-execute, never ask Gemma to try).
#
# main_loop_chtpm.pal's own do_submit sequence runs, in this exact
# order, on EVERY Enter keypress, unconditionally, regardless of
# provider:
#   gemma_strategy -> strategy_execute_a -> send_message
# So this scenario needs NO state seeding at all (unlike an earlier,
# more roundabout version of this test that manually simulated a
# pending-tool-confirmation state) - just type a natural message
# containing a detection keyword and press Enter, exactly like a real
# user would, and the deterministic pipeline does the rest. send_message
# (the actual LLM call) may succeed, fail, or hang depending on network/
# API-key availability - irrelevant here, since strategy_execute_a
# already ran and rendered its result synchronously, BEFORE send_message
# even starts its async curl call.
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
check() { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }

echo "=== muchi-pal-agent REAL pre-Gemma list_dir tool-call scenario ==="
bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
sleep 1

cd "$PROJECT_DIR"
# Real, live-caught race: button.sh run's own reap_stale_sessions() does
# `rm -rf pieces/sessions/*` but only AFTER it starts compiling (~20+
# ops, can take seconds). If a stale session dir from an EARLIER run is
# still sitting there when we start polling below, the poll can lock
# onto that dead directory (it already has a current_frame.txt) instead
# of waiting for the real new one - then reap_stale_sessions deletes it
# out from under us mid-test (keyboard/history.txt, current_frame.txt
# vanish mid-keystroke). Clear it ourselves BEFORE launching so there is
# no stale directory left for the poll to mistake for the real session.
rm -rf "$PROJECT_DIR/pieces/sessions"/*
NO_GL=1 setsid bash button.sh run --pal > /tmp/th_agent_sess.log 2>&1 < /dev/null & disown

# Real, live-caught difference from the other 3 projects' harnesses:
# this project's own orchestrator.c recompiles ALL ~20+ ops on EVERY
# launch ("[Orchestrator] Compiling...") rather than treating compile
# as a separate one-time step - a fixed 3s sleep (which was enough
# everywhere else) was not enough here and current_frame.txt hadn't
# even been created yet by the time the first test attempt tried to
# use it. Poll for real readiness instead of guessing a fixed delay.
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
    fail "session launch - current_frame.txt never appeared within 30s (compile-on-launch may still be running - check /tmp/th_agent_sess.log)"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
STRATEGY_LOG="$SESS/pieces/world_01/session_01/chat/strategy_log.txt"
echo "Session: $SESS"
cp "$FRAME" "$PROOF_DIR/before_message.txt" 2>/dev/null
# Record the strategy_log's line count BEFORE our action - the file is
# copied from a real, long-lived world_01 template with real historical
# entries already in it (confirmed live: entries from a session
# hours earlier) - checking for "our" entry must look at what's NEW,
# not just whether the substring exists anywhere in the file.
BEFORE_LOG_LINES=$(wc -l < "$STRATEGY_LOG" 2>/dev/null || echo 0)

MESSAGE="please list the files in this directory"
echo "--- real keystrokes: activate Message field, type a natural message containing a detection keyword, press Enter ---"
key "$SESS" 13
type_ "$SESS" "$MESSAGE"
key "$SESS" 13
sleep 1.5
cp "$FRAME" "$PROOF_DIR/after_message.txt"
cp "$STRATEGY_LOG" "$PROOF_DIR/strategy_log.txt" 2>/dev/null

echo "--- NEW strategy_log.txt lines from this run only (excludes stale historical entries) ---"
NEW_LOG_LINES=$(tail -n "+$((BEFORE_LOG_LINES + 1))" "$STRATEGY_LOG" 2>/dev/null)
echo "$NEW_LOG_LINES"

if echo "$NEW_LOG_LINES" | grep -q "strategy=A tool=list_dir"; then
    pass "gemma_strategy.+x deterministically detected 'list_dir' from the real typed message JUST NOW (no LLM call involved in this decision) and selected Strategy A (pre-execute, never ask Gemma to try TOOL: format) - confirmed as a genuinely NEW log line, not stale history"
else
    fail "no NEW 'strategy=A tool=list_dir' line appeared in strategy_log.txt after our real keystrokes"
fi
check "$FRAME" "list_dir result" "strategy_execute_a.+x pre-executed the real list_dir.+x binary and its real result was logged + rendered, synchronously, before send_message's own (separate, optional) LLM call ever started"

# KPI #2 (2&3-jul31-sprint I.4) - the regression that actually bit the user:
# the 270M gemma model used to answer ordinary questions with hallucinated
# "TOOL: read file ..." lines because build_gemma_request() replayed old
# assistant|tool_call turns as literal "assistant: TOOL: X {args}" text.
# Fixed 2026-07-31 by (a) skipping tool_call turns in that prompt builder and
# (b) model_after_tool=no (default) skipping the LLM call entirely after a
# pre-run tool. Assert the gemma-hallucination pattern specifically - the
# historical gemini-style "Aida: [calling X {" lines in the copied template
# are a different, legitimate format and stay (user chose leave-history-alone).
echo "--- KPI #2: last 12 frame lines must contain no 'TOOL: <tool> <args>' hallucination ---"
LAST_12=$(tail -12 "$FRAME")
if echo "$LAST_12" | grep -qE "TOOL: (read|write|list|run|search|speak)"; then
    fail "KPI#2: gemma 'TOOL:' hallucination present in the last 12 frame lines"
else
    pass "KPI#2: no gemma 'TOOL:' hallucination in the last 12 frame lines"
fi

# KPI #3 (2&3-jul31-sprint I.5) - display order: the tool result must render
# AFTER the user message that triggered it, not above it. Fixed 2026-07-31 via
# the tool_result.pending handshake: strategy_execute_a stashes the result,
# send_message flushes it into context_log right after the user turn.
echo "--- KPI #3: tool result must render below the triggering user message ---"
USER_LINE=$(grep -n "You: ${MESSAGE}" "$FRAME" | tail -1 | cut -d: -f1)
RESULT_LINE=$(grep -n "list_dir result" "$FRAME" | tail -1 | cut -d: -f1)
if [ -n "$USER_LINE" ] && [ -n "$RESULT_LINE" ] && [ "$RESULT_LINE" -gt "$USER_LINE" ]; then
    pass "KPI#3: list_dir result (line $RESULT_LINE) renders after the user message (line $USER_LINE)"
else
    fail "KPI#3: tool result not after user message (user='$USER_LINE' result='$RESULT_LINE')"
fi

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
