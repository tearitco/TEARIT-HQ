#!/bin/bash
# demo_iqabod_chat.sh - reference SCENARIO for muchi-pal-agent, built
# from test-harn-same/ops/ (same generic tk_* primitives
# demo_list_dir_tool.sh already uses - same launch/poll/cleanup shape,
# copied not reinvented).
#
# Proves PITFALL 60's own fix (2026-07-30: model_list.txt's iqabod
# entries used to hardcode a dead absolute path from a prior house
# reorg; send_message.c's iqabod branch now resolves api_url relative
# to project_root instead) through the REAL, full user-facing path -
# real "/model iqabod-test" keystrokes (not a direct switch_model.+x
# CLI call, which is how this fix was first verified and is a real but
# smaller claim than "the actual chat UI can do this") followed by a
# real chat message, exactly as a human would type both.
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

echo "=== muchi-pal-agent REAL iqabod model-switch + chat scenario ==="
bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
sleep 1

cd "$PROJECT_DIR"
# Same real, live-caught stale-session race demo_list_dir_tool.sh's own
# header comment documents - clear it ourselves before launching.
rm -rf "$PROJECT_DIR/pieces/sessions"/*
NO_GL=1 setsid bash button.sh run --pal > /tmp/th_agent_iqabod_sess.log 2>&1 < /dev/null & disown

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
    fail "session launch - current_frame.txt never appeared within 30s (check /tmp/th_agent_iqabod_sess.log)"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
STATE="$SESS/pieces/world_01/session_01/chat/state.txt"
CONTEXT_LOG="$SESS/pieces/world_01/session_01/chat/context_log.txt"
echo "Session: $SESS"
cp "$FRAME" "$PROOF_DIR/00_before.txt" 2>/dev/null
BEFORE_CTX_LINES=$(wc -l < "$CONTEXT_LOG" 2>/dev/null || echo 0)

echo "--- real keystrokes: switch model via /model iqabod-test, exactly as a human would type it ---"
key "$SESS" 13
type_ "$SESS" "/model iqabod-test"
key "$SESS" 13
sleep 1
cp "$STATE" "$PROOF_DIR/01_state_after_switch.txt"

MODEL_ID=$(grep '^current_model_id=' "$STATE" 2>/dev/null | cut -d= -f2)
PROVIDER=$(grep '^provider_kind=' "$STATE" 2>/dev/null | cut -d= -f2)
API_URL=$(grep '^current_api_url=' "$STATE" 2>/dev/null | cut -d= -f2)
echo "current_model_id=$MODEL_ID provider_kind=$PROVIDER current_api_url=$API_URL"

if [ "$MODEL_ID" = "iqabod-test" ] && [ "$PROVIDER" = "iqabod" ]; then
    pass "real /model keystrokes switched the live session to iqabod-test (provider_kind=iqabod confirmed in state.txt)"
else
    fail "model switch did not take - current_model_id=$MODEL_ID provider_kind=$PROVIDER"
fi

case "$API_URL" in
    /*) fail "current_api_url is still an absolute path ($API_URL) - PITFALL 60's own fix did not take, or model_list.txt regressed" ;;
    *)  pass "current_api_url is relative ($API_URL), not the old dead absolute ZEST-10.00 path - PITFALL 60's own fix confirmed live" ;;
esac

echo "--- real keystrokes: type a chat message, press Enter ---"
key "$SESS" 13
type_ "$SESS" "hello how are you"
key "$SESS" 13

echo "--- poll for ai_state to leave THINKING (real fork + real generation, not instant) ---"
RESOLVED=0
for i in $(seq 1 20); do
    AI_STATE=$(grep '^ai_state=' "$STATE" 2>/dev/null | cut -d= -f2)
    if [ "$AI_STATE" != "THINKING" ]; then RESOLVED=1; break; fi
    sleep 1
done
cp "$STATE" "$PROOF_DIR/02_state_after_chat.txt"
cp "$CONTEXT_LOG" "$PROOF_DIR/02_context_log.txt" 2>/dev/null
cp "$FRAME" "$PROOF_DIR/02_after_chat_frame.txt" 2>/dev/null

if [ "$RESOLVED" = "1" ] && [ "$AI_STATE" = "IDLE" ]; then
    pass "ai_state cycled THINKING -> IDLE (real fork of main_orchestrator.+x completed, chdir() succeeded - PITFALL 60's own dead-path failure mode, _exit(127) with ai_state stuck, did NOT reproduce)"
else
    fail "ai_state never resolved to IDLE within 20s (got '$AI_STATE') - possible regression of PITFALL 60's own fix"
fi

echo "--- NEW context_log.txt lines from this run only (excludes stale historical entries) ---"
NEW_CTX_LINES=$(tail -n "+$((BEFORE_CTX_LINES + 1))" "$CONTEXT_LOG" 2>/dev/null)
echo "$NEW_CTX_LINES"

if echo "$NEW_CTX_LINES" | grep -q "^assistant|text||"; then
    pass "a real assistant|text|| reply landed in context_log.txt from this run's own real generation (not stale history, not empty)"
else
    fail "no new assistant|text|| line appeared in context_log.txt after the real chat message"
fi

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
