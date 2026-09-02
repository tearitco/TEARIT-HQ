#!/bin/bash
# demo_2wallet_ux.sh - reference SCENARIO for pal-chain, built from
# test-harn-same/ops/ (same generic tk_* primitives 044.pal-chat-irc's
# own harness uses - no project-specific logic in the ops themselves).
#
# Proves the REAL key-injected UX pipeline works for 2 concurrent
# same-install wallet sessions: signup -> login -> wallet -> mine a
# real block -> send real millicones -> recipient's balance increases,
# via actual keystrokes through the actual menu system (not direct op
# invocation). Uses CHAIN_DIFFICULTY_HEX_ZEROS=1 (chain_miner.c's own
# documented override, no code change needed) so mining resolves in
# well under a second instead of the ~1M-try default difficulty -
# see #.haiku+/!.xyzos-pitfalls+1.txt for why a fresh wallet can't send
# anything without this (starts at 0 balance, no faucet/genesis grant).
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
    # Defense in depth (real, live-caught bug building this scenario):
    # a UI-driven "Stop Mining" click can silently fail if the session
    # isn't on wallet_main.chtpm at that moment (send_screen.chtpm has
    # no such button) - chain_miner.+x then keeps running at ~100% CPU
    # forever since it has no other stop condition. Kill it directly by
    # pattern here too, don't rely solely on the UI click having worked.
    pkill -9 -f "ops/\+x/chain_miner" 2>/dev/null
    bash "$HARNESS_DIR/button.sh" kill
}
trap cleanup EXIT

key()    { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.2; }
type_()  { "$OPS/tk_type_text.+x" "$1" "$2"; sleep 0.2; }
focus()  { "$OPS/tk_focus_item.+x" "$1" "$2" "$3" >/dev/null; sleep 0.2; }
check()  { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }
# Real, live-caught bug (found building this exact scenario): a cli_io
# field can carry OVER content from a previous screen visit (target_id
# persistence, e.g. wallet_id_input surviving Back-to-Login after
# signup) - re-activating it and typing again APPENDS to that existing
# buffer instead of replacing it ("walletAwalletA"), silently producing
# a wrong value with no error. Always clear first: 20 backspaces is
# more than any field here ever holds.
fill_field() {
    local sess="$1" frame="$2" label="$3" value="$4"
    focus "$sess" "$frame" "$label"; key "$sess" 13
    for _ in $(seq 1 20); do "$OPS/tk_inject_key.+x" "$sess" 127; done
    type_ "$sess" "$value"; key "$sess" 27
}

signup_and_login() {
    local sess="$1" frame="$2" wallet="$3" pw="$4"
    # login screen: go to signup
    focus "$sess" "$frame" "Create New Wallet"; key "$sess" 13
    sleep 0.5
    # signup screen: fill wallet id + password, create
    fill_field "$sess" "$frame" "Wallet ID" "$wallet"
    fill_field "$sess" "$frame" "Password" "$pw"
    focus "$sess" "$frame" "Create Wallet"; key "$sess" 13
    sleep 0.5
    check "$frame" "created" "wallet '$wallet' created"
    # back to login, then log in for real
    focus "$sess" "$frame" "Back to Login"; key "$sess" 13
    sleep 0.5
    fill_field "$sess" "$frame" "Wallet ID" "$wallet"
    fill_field "$sess" "$frame" "Password" "$pw"
    focus "$sess" "$frame" "Log In"; key "$sess" 13
    sleep 0.5
    check "$frame" "Logged in" "wallet '$wallet' logged in"
    focus "$sess" "$frame" "Continue to Wallet"; key "$sess" 13
    sleep 0.5
    check "$frame" "M Y   W A L L E T" "wallet '$wallet' reached wallet_main"
}

echo "=== pal-chain 2-wallet real key-injected UX scenario ==="
bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
sleep 1

cd "$PROJECT_DIR"
# Real, live-caught issue (found running this scenario repeatedly): at
# CHAIN_DIFFICULTY_HEX_ZEROS=1, mining races through thousands of
# blocks per second - the halving schedule (reward_for_block in
# chain_miner.c, reward hits 0 after enough blocks/epochs) gets
# exhausted within just 2-3 repeated runs of this scenario, after which
# EVERY future run's mining silently idles forever (chain_miner.c's own
# "reward <= 0 -> idle" branch) with no error, just no balance ever
# appearing - looks exactly like a hang/regression if you don't know to
# check block count. Reset (backup, don't destroy) to a fresh chain at
# the start of every run so this scenario stays repeatable indefinitely
# regardless of how many times it's been run before.
if [ -f "data/blockchain.txt" ] && [ -s "data/blockchain.txt" ]; then
    cp "data/blockchain.txt" "data/blockchain.txt.pre-harness-run-$(date +%Y%m%d-%H%M%S)"
    : > "data/blockchain.txt"
    echo "(reset data/blockchain.txt to a fresh chain for this run - previous chain backed up alongside it)"
fi
export CHAIN_DIFFICULTY_HEX_ZEROS=1
NO_GL=1 setsid bash button.sh run > /tmp/th_chain_sess_a.log 2>&1 < /dev/null & disown
sleep 1
NO_GL=1 setsid bash button.sh run > /tmp/th_chain_sess_b.log 2>&1 < /dev/null & disown
sleep 3

SESS_A=$(ls -dt pieces/sessions/*/ | sed -n '2p'); SESS_A="${SESS_A%/}"
SESS_B=$(ls -dt pieces/sessions/*/ | sed -n '1p'); SESS_B="${SESS_B%/}"
FRAME_A="$SESS_A/pieces/display/current_frame.txt"
FRAME_B="$SESS_B/pieces/display/current_frame.txt"
WALLET_A="walletA_$$"
WALLET_B="walletB_$$"
PW="testpw123"

echo "--- wallet A ($WALLET_A): signup + login + reach wallet_main ---"
signup_and_login "$SESS_A" "$FRAME_A" "$WALLET_A" "$PW"
cp "$FRAME_A" "$PROOF_DIR/walletA_at_wallet_main.txt"

echo "--- wallet B ($WALLET_B): signup + login + reach wallet_main (SAME install, concurrent) ---"
signup_and_login "$SESS_B" "$FRAME_B" "$WALLET_B" "$PW"
cp "$FRAME_B" "$PROOF_DIR/walletB_at_wallet_main.txt"

echo "--- wallet A starts mining (real click, low difficulty for fast test) ---"
focus "$SESS_A" "$FRAME_A" "Start Mining"; key "$SESS_A" 13
sleep 0.5
cp "$FRAME_A" "$PROOF_DIR/walletA_mining_started.txt"
check "$FRAME_A" "Mining started" "wallet A's real click started chain_miner.+x"

echo "--- waiting for at least one real block to be mined (polling actual balance, up to 20s) ---"
MINED=0
for i in $(seq 1 20); do
    sleep 1
    BAL=$(PRISC_PROJECT_ROOT="$PROJECT_DIR" ./ops/+x/chain_balance.+x "$WALLET_A" 2>/dev/null)
    if [ -n "$BAL" ] && [ "$BAL" -gt 0 ] 2>/dev/null; then
        MINED=1
        echo "wallet A balance after ${i}s: $BAL millicones"
        break
    fi
done
if [ "$MINED" = "1" ]; then
    pass "wallet A mined a real block (balance went from 0 to $BAL millicones via chain_balance.+x, directly observed)"
else
    fail_note="wallet A never mined a block within 20s (CHAIN_DIFFICULTY_HEX_ZEROS=1 should make this near-instant) - check /tmp/pal_chain_miner.log"
    echo "FAIL: $fail_note"
    FAIL=1
fi

# IMPORTANT (real, live-caught behavior): chain_send.c only writes a
# PENDING transaction - it is NOT applied to any balance until a miner
# actually includes it in a mined block (read_pending_tx() in
# chain_miner.c's own mining loop). Stopping mining before the send (the
# first version of this scenario did that "to avoid racing the send")
# means the pending tx never gets mined in - wallet B's balance stayed
# at 0 forever. Mining must stay ACTIVE through and briefly after the
# send so the next block picks the pending tx up.
echo "--- wallet A sends real millicones to wallet B (mining still active, so the tx gets picked up into the next block) ---"
focus "$SESS_A" "$FRAME_A" "Send"; key "$SESS_A" 13
sleep 0.5
fill_field "$SESS_A" "$FRAME_A" "Recipient Wallet ID" "$WALLET_B"
fill_field "$SESS_A" "$FRAME_A" "Amount" "1"
focus "$SESS_A" "$FRAME_A" "Send Cones"; key "$SESS_A" 13
sleep 1
cp "$FRAME_A" "$PROOF_DIR/walletA_send_result.txt"
# NOTE: chain_send.c's confirmation used to also repeat "from $WALLET_A"
# and a full tx_id - that made this line silently overflow the frame's
# fixed BOX_W=60 box and truncate the recipient wallet ID (real bug,
# fixed 2026-07-30 - see chain_send.c and chain_compose_frame.c). The
# sender wallet is already shown on its own "Wallet: ..." line above,
# so it was dropped from the confirmation text.
check "$FRAME_A" "Sent 1 millicones to $WALLET_B" "wallet A sent 1 real millicone to wallet B via real keystrokes (typed recipient+amount, real chain_send.c wrote a real pending transaction)"

echo "--- waiting for the pending transaction to be mined into a real block (polling wallet B's real balance, up to 10s) ---"
RECEIVED=0
for i in $(seq 1 10); do
    sleep 1
    BAL_B=$(PRISC_PROJECT_ROOT="$PROJECT_DIR" ./ops/+x/chain_balance.+x "$WALLET_B" 2>/dev/null)
    if [ -n "$BAL_B" ] && [ "$BAL_B" -ge 1 ] 2>/dev/null; then
        RECEIVED=1
        echo "wallet B balance after ${i}s: $BAL_B millicones"
        break
    fi
done

# still on send_screen.chtpm after the send - "Stop Mining" only exists
# on wallet_main.chtpm, must navigate back first.
focus "$SESS_A" "$FRAME_A" "Back to Wallet"; key "$SESS_A" 13
sleep 0.5
focus "$SESS_A" "$FRAME_A" "Stop Mining"; key "$SESS_A" 13
sleep 0.5

echo "--- wallet B independently checks its own balance via the real UI (not just the CLI probe above) ---"
focus "$SESS_B" "$FRAME_B" "Check Balance"; key "$SESS_B" 13
sleep 1
cp "$FRAME_B" "$PROOF_DIR/walletB_balance_after_receive.txt"

if [ "$RECEIVED" = "1" ]; then
    pass "wallet B received the real transfer - balance is $BAL_B millicones (was 0 before A's send, confirmed via both direct chain_balance.+x probe and B's own real UI Check Balance click)"
else
    fail "wallet B's balance did not reflect the transfer within 10s (got '$BAL_B') - check whether mining was still active when the tx was sent"
fi

cp "$SESS_A/debug/frame_history.txt" "$PROOF_DIR/walletA_frame_history.txt" 2>/dev/null
cp "$SESS_B/debug/frame_history.txt" "$PROOF_DIR/walletB_frame_history.txt" 2>/dev/null

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
