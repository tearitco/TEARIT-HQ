#!/bin/bash
# demo_login_screen_smoke.sh - reference SCENARIO for myne-qrypto/qtc.
#
# SCOPE, STATED HONESTLY: this is a SMOKE TEST, not a full login/signup/
# mine flow. It proves the thing that was genuinely unverified before
# this harness existed - that 041.pal-chain⛓️'s real, unmodified engine
# actually LAUNCHES and RENDERS correctly from its new location
# (@.apps/myne-qrypto/qtc/, fresh empty data/+wallets/, its own session-
# isolation) - not just that the binaries compile (that was already
# confirmed via `button.sh check` before this harness existed).
#
# A full signup->login->mine flow needs real multi-field <cli_io>
# navigation (wallet_id_input, then password_input, each its own
# focusable nav item) whose exact chtpm_parser_pal.c focus-index
# behavior was NOT confirmed directly this session - building that
# blind risked a harness that LOOKS like it tests the real flow but
# actually asserts on the wrong thing. Left as real, valuable, un-done
# follow-up work (see this scenario's own final echo block) rather than
# guessed at.
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
    (cd "$PROJECT_DIR" && bash button.sh kill 2>/dev/null)
    rm -rf "$PROJECT_DIR/pieces/sessions"
}
trap cleanup EXIT

check() { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }

echo "=== myne-qrypto/qtc login-screen launch smoke test ==="
(cd "$PROJECT_DIR" && bash button.sh kill >/dev/null 2>&1)
sleep 1

cd "$PROJECT_DIR"
rm -rf "$PROJECT_DIR/pieces/sessions"

bash button.sh run < /dev/null > /tmp/th_qtc_sess.log 2>&1 &
disown

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
    fail "session launch - current_frame.txt never appeared within 30s (check /tmp/th_qtc_sess.log)"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
echo "Session: $SESS"
cp "$FRAME" "$PROOF_DIR/login_frame.txt" 2>/dev/null

echo "--- real process health ---"
for proc in "system/orchestrator" "system/keyboard_input" "system/chtpm_parser_pal" "prisc\+x"; do
    if pgrep -f "$proc" > /dev/null 2>&1; then
        pass "process alive: $proc"
    else
        fail "process NOT found: $proc"
    fi
done

echo "--- login screen render content (the real unverified thing) ---"
check "$FRAME" "P A L - C H A I N" "login screen title rendered"
check "$FRAME" "Wallet ID" "wallet ID cli_io field label rendered"
check "$FRAME" "Password" "password cli_io field label rendered"
check "$FRAME" "Create New Wallet" "signup nav button rendered"
check "$FRAME" "Continue to Wallet" "wallet nav button rendered"

echo "--- CPU sanity check (Pitfall 22/51 - keyboard_input must not be busy-spinning) ---"
KB_PID=$(pgrep -f "system/keyboard_input" | head -1)
if [ -n "$KB_PID" ]; then
    CPU=$(ps -o %cpu= -p "$KB_PID" 2>/dev/null | tr -d ' ')
    if [ -n "$CPU" ] && awk "BEGIN{exit !($CPU < 20)}"; then
        pass "keyboard_input CPU usage low ($CPU%) - Pitfall 51 fix holding"
    else
        fail "keyboard_input CPU usage high ($CPU%) - Pitfall 51 fix may have regressed"
    fi
else
    fail "keyboard_input process not found for CPU check"
fi

echo
echo "=== proof saved to: $PROOF_DIR ==="
echo "=== KNOWN GAP, NOT A FAILURE: no signup/login/mine flow tested yet ==="
echo "=== (multi-field cli_io focus mechanics need confirming first - see"
echo "===  this script's own header comment)"
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
