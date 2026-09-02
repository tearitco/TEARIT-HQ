#!/bin/bash
# demo_model_remember.sh - proves "remember the user's last chosen model and
# default to it; first default = gemma-lan" (2&3-jul31-sprint, 2026-07-31).
#
# Two REAL `button.sh run --pal` boots (each with the project's own full
# compile-on-launch + real injected keystrokes, exactly like a human):
#   Run 1: with last_model.txt empty -> boots `gemma-lan` (the first
#          default, from the world_01 template state.txt). Real
#          "/model groq-tool-use-mac" keystrokes -> the live session's
#          state.txt updates AND switch_model.c writes the real top-level
#          last_model.txt.
#   kill -> Run 2: boots `groq-tool-use-mac` (remembered across the session
#          wipe that every launch performs - reap_stale_sessions rm -rf
#          pieces/sessions/* - so this must come from last_model.txt, not
#          from any session carry-over).
#
# last_model.txt and the world_01 template state.txt are snapshotted up
# front and restored in cleanup, so the scenario is repeatable and leaves
# the house exactly as it found it.
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
OPS="$HARNESS_DIR/ops/+x"

PROOF_DIR="$PROJECT_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

LAST_FILE="$PROJECT_DIR/last_model.txt"
TEMPLATE_STATE="$PROJECT_DIR/pieces/world_01/session_01/chat/state.txt"

BACKUP_DIR="$(mktemp -d /tmp/mr_backup.XXXXXX)"
cp "$LAST_FILE" "$BACKUP_DIR/last_model.txt" 2>/dev/null || : > "$BACKUP_DIR/last_model.txt"
cp "$TEMPLATE_STATE" "$BACKUP_DIR/template_state.txt"

cleanup() {
    echo; echo "--- cleanup ---"
    bash "$HARNESS_DIR/button.sh" kill
    cp "$BACKUP_DIR/last_model.txt" "$LAST_FILE"
    cp "$BACKUP_DIR/template_state.txt" "$TEMPLATE_STATE"
    rm -rf "$BACKUP_DIR"
}
trap cleanup EXIT

key()   { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.2; }
type_() { "$OPS/tk_type_text.+x" "$1" "$2"; sleep 0.2; }

# launch_and_poll - wipe stale sessions, boot the real app, wait for the
# real current_frame.txt, echo the session dir (or nothing on timeout).
launch_and_poll() {
    rm -rf "$PROJECT_DIR/pieces/sessions"/*
    NO_GL=1 setsid bash "$PROJECT_DIR/button.sh" run --pal > /tmp/th_model_remember_sess.log 2>&1 < /dev/null & disown
    local s
    for i in $(seq 1 40); do
        s="$(ls -dt "$PROJECT_DIR"/pieces/sessions/*/ 2>/dev/null | head -1)"
        if [ -n "$s" ] && [ -f "${s}pieces/display/current_frame.txt" ]; then
            echo "${s%/}"
            return 0
        fi
        sleep 1
    done
    return 1
}

echo "=== muchi-pal-agent model-remember scenario (gemma-lan first default, then remember) ==="
bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
sleep 1

echo "--- Run 1: empty last_model.txt, expect the FIRST default = gemma-lan ---"
: > "$LAST_FILE"
SESS1="$(launch_and_poll)"
if [ -z "$SESS1" ]; then fail "run 1 - session never became ready"; exit 1; fi
STATE1="$SESS1/pieces/world_01/session_01/chat/state.txt"
echo "Run 1 session: $SESS1"
cp "$SESS1/pieces/display/current_frame.txt" "$PROOF_DIR/run1_boot.txt" 2>/dev/null

MODEL1="$(grep '^current_model_id=' "$STATE1" 2>/dev/null | cut -d= -f2)"
if [ "$MODEL1" = "gemma-lan" ]; then
    pass "first default model is gemma-lan (empty last_model.txt, template state wins)"
else
    fail "expected first default gemma-lan, got '$MODEL1'"
fi

echo "--- real keystrokes: /model groq-tool-use-mac, exactly as a human would type it ---"
key "$SESS1" 13
type_ "$SESS1" "/model groq-tool-use-mac"
key "$SESS1" 13
sleep 1
cp "$SESS1/pieces/display/current_frame.txt" "$PROOF_DIR/run1_after_switch.txt" 2>/dev/null

MODEL2="$(grep '^current_model_id=' "$STATE1" 2>/dev/null | cut -d= -f2)"
if [ "$MODEL2" = "groq-tool-use-mac" ]; then
    pass "real /model keystrokes switched the live session to groq-tool-use-mac"
else
    fail "model switch did not take - current_model_id='$MODEL2'"
fi
LAST_WRITTEN="$(tr -d ' \n' < "$LAST_FILE")"
if [ "$LAST_WRITTEN" = "groq-tool-use-mac" ]; then
    pass "switch_model.c persisted the choice to the real top-level last_model.txt"
else
    fail "last_model.txt contains '$LAST_WRITTEN', expected groq-tool-use-mac"
fi

echo "--- kill, then Run 2: expect the remembered model to survive the session wipe ---"
bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
sleep 1
SESS2="$(launch_and_poll)"
if [ -z "$SESS2" ]; then fail "run 2 - session never became ready"; exit 1; fi
STATE2="$SESS2/pieces/world_01/session_01/chat/state.txt"
echo "Run 2 session: $SESS2"
cp "$SESS2/pieces/display/current_frame.txt" "$PROOF_DIR/run2_boot.txt" 2>/dev/null

MODEL3="$(grep '^current_model_id=' "$STATE2" 2>/dev/null | cut -d= -f2)"
PROVIDER="$(grep '^provider_kind=' "$STATE2" 2>/dev/null | cut -d= -f2)"
if [ "$MODEL3" = "groq-tool-use-mac" ]; then
    pass "second boot remembered the last chosen model (groq-tool-use-mac) across a full session wipe"
else
    fail "second boot did NOT remember - current_model_id='$MODEL3'"
fi
if [ "$PROVIDER" = "ollama" ]; then
    pass "remembered provider_kind=ollama too (state stamped as a full row, not just an id)"
else
    fail "provider_kind='$PROVIDER', expected ollama"
fi

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
