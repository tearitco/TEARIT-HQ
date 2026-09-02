#!/bin/bash
# demo_signup_login_wallet.sh - the REAL flow demo_login_screen_smoke.sh
# named as a known gap, now filled: create a wallet, log in with it,
# reach the real wallet screen - all through real key injection against
# the real <cli_io> multi-field mechanic, confirmed by direct read of
# system/chtpm_parser_pal.c (not guessed):
#
#   1. cli_io fields ARE numbered nav items, same numbered list as
#      buttons (is_interactive() includes cli_io - confirmed line
#      ~2205) - tk_focus_item.c's digit-jump-by-label mechanism works
#      on them unmodified, because cli_io renders with the exact same
#      "[pref] N. Label: [value]" bracket+number format as everything
#      else navigable (confirmed line ~2916-2924).
#   2. Enter (13) while a cli_io field is FOCUSED (nav mode) ACTIVATES
#      it for typing (confirmed line ~3310-3315).
#   3. While ACTIVE, each printable keystroke appends to the field's
#      input_buffer AND live-syncs to gui_state.txt under that field's
#      own target_id key on EVERY keystroke, not just on commit
#      (confirmed line ~3494-3507) - this is what chain_menu_input.c's
#      LOGIN/SIGNUP handlers read back out via read_gui_state_str().
#   4. Enter again while ACTIVE with a non-empty buffer COMMITS (syncs
#      + clears the buffer) but explicitly does NOT deactivate ("STAY
#      ACTIVE: Do NOT deactivate the input element", confirmed line
#      ~3480) - so Enter is NOT how you move to the next field.
#   5. ESC (27) is what deactivates a cli_io field, and per its own
#      "KISS: ESC just deactivates, NEVER clears input" comment
#      (confirmed line ~3378-3391), the typed value is NOT lost - it
#      was already live-synced to gui_state.txt in step 3, before ESC
#      is ever pressed.
#
# So the real per-field fill sequence is: digit-jump to field -> Enter
# (activate) -> type characters -> ESC (deactivate, value preserved) ->
# digit-jump to the NEXT field. This was NOT known/confirmed when
# demo_login_screen_smoke.sh was first written (that scenario's own
# header comment named this exact gap) - now it is, and this scenario
# is the fill.
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

key()   { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.3; }
type_() { "$OPS/tk_type_text.+x" "$1" "$2"; sleep 0.3; }
check() { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }
# REAL, LIVE-CAUGHT BUG in this scenario's first draft: a <cli_io
# target_id="wallet_id_input"> field's typed value PERSISTS across
# screens sharing that same target_id (real, confirmed chtpm behavior -
# login.chtpm and signup.chtpm both use target_id="wallet_id_input").
# Re-typing into an already-filled field APPENDS instead of replacing,
# producing garbage ("harnesstestNharnesstestN"). Clear first with
# enough backspaces (30 covers any wallet_id/password this scenario
# uses) so typing is correct regardless of what was there before -
# password_input does NOT appear to persist across screens (observed
# empty on return-to-login), but clearing unconditionally is robust to
# either case and matches how a careful human would behave anyway.
clear_field() {
    local i
    for i in $(seq 1 30); do
        "$OPS/tk_inject_key.+x" "$1" 127
    done
    sleep 0.3
}
focus() {
    # $1=session $2=frame $3=label -> returns nothing, just sends the
    # digit-jump keystrokes (tk_focus_item.c injects them itself)
    local out
    out=$("$OPS/tk_focus_item.+x" "$1" "$2" "$3" 2>&1)
    if [ $? -ne 0 ]; then
        fail "focus '$3' - $out"
        return 1
    fi
    sleep 0.3
    return 0
}

WALLET_ID="harnesstest$(date +%s)"
PASSWORD="testpass123"

echo "=== myne-qrypto/qtc REAL signup -> login -> wallet screen scenario ==="
echo "Using wallet_id=$WALLET_ID"
(cd "$PROJECT_DIR" && bash button.sh kill >/dev/null 2>&1)
sleep 1

cd "$PROJECT_DIR"
rm -rf "$PROJECT_DIR/pieces/sessions"

# setsid: run the session in its OWN process group, so the death-sweep
# group-kill in the session's cleanup cascade can't take THIS scenario
# down with it when we end the session in step 8.
setsid bash button.sh run < /dev/null > /tmp/th_qtc_flow_sess.log 2>&1 &
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
    fail "session launch - current_frame.txt never appeared within 30s"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
echo "Session: $SESS"
cp "$FRAME" "$PROOF_DIR/01_login_initial.txt" 2>/dev/null

echo "--- step 1: navigate Login -> Signup ---"
focus "$SESS" "$FRAME" "Create New Wallet" || exit 1
key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/02_signup_screen.txt" 2>/dev/null
check "$FRAME" "C R E A T E   W A L L E T" "landed on signup screen"

echo "--- step 2: fill Wallet ID field ---"
focus "$SESS" "$FRAME" "Wallet ID" || exit 1
key "$SESS" 13
clear_field "$SESS"
type_ "$SESS" "$WALLET_ID"
key "$SESS" 27
sleep 0.5
cp "$FRAME" "$PROOF_DIR/03_after_wallet_id_typed.txt" 2>/dev/null
check "$FRAME" "$WALLET_ID" "wallet ID value visible in signup frame after typing"

echo "--- step 3: fill Password field ---"
focus "$SESS" "$FRAME" "Password" || exit 1
key "$SESS" 13
clear_field "$SESS"
type_ "$SESS" "$PASSWORD"
key "$SESS" 27
sleep 0.5
cp "$FRAME" "$PROOF_DIR/04_after_password_typed.txt" 2>/dev/null

echo "--- step 4: submit Create Wallet ---"
focus "$SESS" "$FRAME" "Create Wallet" || exit 1
key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/05_after_signup_submit.txt" 2>/dev/null
check "$FRAME" "created" "signup succeeded (real chain_create_wallet.+x ran)"

echo "--- ground truth: wallet dir on disk (not just UI text) ---"
# wallets/<wallet_id>/ is a DIRECTORY containing wallet.txt (confirmed
# live this session - NOT a flat wallets/<wallet_id> file, an earlier
# draft of this assertion assumed).
# Step 2 symlink-migration note: chain_create_wallet writes via
# project_root = the SESSION copy, so mid-run disk truth lives under
# $SESS/wallets/ (pre-migration the session dir was symlinks, so this
# check against the real root passed incidentally). Real-root
# persistence is asserted after exit in step 8 below.
if [ -d "$SESS/wallets/$WALLET_ID" ] && [ -f "$SESS/wallets/$WALLET_ID/wallet.txt" ]; then
    pass "wallet dir+file exists on disk (session): wallets/$WALLET_ID/wallet.txt"
else
    fail "no wallet dir/file found at session wallets/$WALLET_ID/wallet.txt"
fi

echo "--- step 5: back to Login ---"
focus "$SESS" "$FRAME" "Back to Login" || exit 1
key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/06_back_at_login.txt" 2>/dev/null
check "$FRAME" "P A L - C H A I N" "landed back on login screen"

echo "--- step 6: fill Wallet ID + Password again, submit Log In ---"
focus "$SESS" "$FRAME" "Wallet ID" || exit 1
key "$SESS" 13
clear_field "$SESS"
type_ "$SESS" "$WALLET_ID"
key "$SESS" 27
sleep 0.3
focus "$SESS" "$FRAME" "Password" || exit 1
key "$SESS" 13
clear_field "$SESS"
type_ "$SESS" "$PASSWORD"
key "$SESS" 27
sleep 0.3
focus "$SESS" "$FRAME" "Log In" || exit 1
key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/07_after_login_submit.txt" 2>/dev/null
check "$FRAME" "Logged in" "login succeeded (real chain_login.+x ran, real SHA-256 password check passed)"

echo "--- step 7: continue to real wallet screen ---"
focus "$SESS" "$FRAME" "Continue to Wallet" || exit 1
key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/08_wallet_screen.txt" 2>/dev/null
check "$FRAME" "M Y   W A L L E T" "landed on real wallet_main screen"
check "$FRAME" "$WALLET_ID" "wallet screen shows our real logged-in wallet_id"

if grep -q "Not logged in" "$FRAME" 2>/dev/null; then
    fail "wallet screen says 'Not logged in' - session did not actually carry through"
else
    pass "wallet screen does NOT say 'Not logged in' - real session carried through"
fi

echo "--- step 8: exit session, verify wallet persisted to real root ---"
# Step 2 symlink-migration: the session's wallets/ only reaches the real
# root via button.sh's persist_session_state() copy-back at cleanup.
# NOTE: do NOT use `button.sh kill` here - it sweeps everything under the
# qtc project (kill_all.sh) and kills THIS scenario along with it. Kill
# only this session's foreground keyboard_input (cwd match) so the run
# action's own EXIT trap fires naturally.
SDABS="$(cd "$SESS" && pwd)"
for pid in $(pgrep -f "system/keyboard_input"); do
    kcwd=$(readlink -f "/proc/$pid/cwd" 2>/dev/null)
    [ "$kcwd" = "$SDABS" ] && kill -TERM "$pid" 2>/dev/null
done
sleep 3
if [ -d "$PROJECT_DIR/wallets/$WALLET_ID" ] && [ -f "$PROJECT_DIR/wallets/$WALLET_ID/wallet.txt" ]; then
    pass "wallet persisted to REAL wallets/ after session exit (Step 2 copy-back)"
else
    fail "wallet did NOT persist to real wallets/ after session exit"
fi

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
