#!/bin/bash
# demo_automation.sh - reference SCENARIO for my-chara-txt's
# automation/supervision layer (Manual/Semi/Full), reproducing this
# session's own live-verified manual trace (see
# HANDOFF_NEXT_SESSION.md §2) as a real re-runnable regression test.
#
# Deliberately stays on the automation.chtpm screen for the ENTIRE
# scenario (never navigates to main/farm/mine) - see demo_farm_mine.sh's
# own header comment for the real, confirmed interact_relay.txt
# consumption-position bug that compounds with EVERY screen navigation.
# Staying on one screen throughout sidesteps that bug entirely rather
# than fighting it, and happens to match how a real player would
# actually use this screen (set a mode, watch it work, no reason to
# leave).
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

key() { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.3; }
check() { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }

echo "=== my-chara-txt REAL Automation (Manual/Semi/Full) regression scenario ==="
(cd "$PROJECT_DIR" && bash button.sh kill >/dev/null 2>&1)
sleep 1

cd "$PROJECT_DIR"
rm -rf "$PROJECT_DIR/pieces/sessions"
rm -f "$PROJECT_DIR/data/master_ledger.txt"
cat > "$PROJECT_DIR/pieces/system/config.txt" << 'EOCONFIG'
game_id=my-chara-001
player_name=Adam
day=1
max_days=10
health=100
money=500
grain_in_inventory=10
silver_in_inventory=0
gold_in_inventory=0
game_state=playing
supervision_mode=manual
decision_mode=0
risk_level=5
compute_tier=0
paused_for_confirmation=0
last_auto_tick=0
EOCONFIG
cat > "$PROJECT_DIR/pieces/system/plots.txt" << 'EOPLOTS'
plot_0_state=empty
plot_0_crop=
plot_0_harvest_day=0
plot_1_state=empty
plot_1_crop=
plot_1_harvest_day=0
plot_2_state=empty
plot_2_crop=
plot_2_harvest_day=0
EOPLOTS

NO_GL=1 bash button.sh run < /dev/null > /tmp/th_mychara_auto_sess.log 2>&1 &
disown

SESS=""
for i in $(seq 1 30); do
    CANDIDATE=$(ls -dt pieces/sessions/*/ 2>/dev/null | head -1)
    if [ -n "$CANDIDATE" ] && grep -q "M Y - C H A R A" "${CANDIDATE}pieces/display/current_frame.txt" 2>/dev/null; then
        SESS="${CANDIDATE%/}"
        break
    fi
    sleep 1
done

if [ -z "$SESS" ]; then
    fail "session launch - real frame never appeared within 30s (check /tmp/th_mychara_auto_sess.log)"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
CONFIG="$PROJECT_DIR/pieces/system/config.txt"
LEDGER="$PROJECT_DIR/data/master_ledger.txt"
echo "Session: $SESS"

echo "--- navigate main -> Automation (item 3) - the ONLY navigation" \
     "this whole scenario does ---"
key "$SESS" 51; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/00_automation_screen.txt" 2>/dev/null
check "$FRAME" "M Y - C H A R A   [automation]" "navigated to automation screen"
check "$FRAME" "Supervision: manual" "baseline supervision is manual"

echo "--- MANUAL sanity: confirm zero drift while idle ---"
sleep 2
check "$CONFIG" "day=1" "day unchanged after idling in manual mode"
check "$CONFIG" "last_auto_tick=0" "last_auto_tick untouched in manual mode"

echo "--- SEMI: exactly one auto action, then pause ---"
key "$SESS" 50; key "$SESS" 13
sleep 1.5
cp "$FRAME" "$PROOF_DIR/01_semi_after_one_action.txt" 2>/dev/null
check "$CONFIG" "supervision_mode=semi" "supervision set to semi"
check "$CONFIG" "paused_for_confirmation=1" "paused after exactly one auto action"
LEDGER_LINES_SEMI1=$(wc -l < "$LEDGER" 2>/dev/null || echo 0)
if [ "$LEDGER_LINES_SEMI1" -eq 1 ]; then
    pass "exactly one ledger line after Semi's first auto action"
else
    fail "expected 1 ledger line after Semi's first action, got $LEDGER_LINES_SEMI1"
fi

echo "--- SEMI stays paused (no second action without Continue) ---"
sleep 2
LEDGER_LINES_STILL=$(wc -l < "$LEDGER" 2>/dev/null || echo 0)
if [ "$LEDGER_LINES_STILL" -eq 1 ]; then
    pass "still exactly one ledger line - Semi correctly stayed paused"
else
    fail "ledger grew to $LEDGER_LINES_STILL lines while paused - Semi did not stay paused"
fi

echo "--- SEMI: Continue triggers exactly one more action, then re-pauses ---"
key "$SESS" 52; key "$SESS" 13
sleep 1.5
cp "$FRAME" "$PROOF_DIR/02_semi_after_continue.txt" 2>/dev/null
check "$CONFIG" "paused_for_confirmation=1" "re-paused after Continue's one action"
LEDGER_LINES_SEMI2=$(wc -l < "$LEDGER" 2>/dev/null || echo 0)
if [ "$LEDGER_LINES_SEMI2" -eq 2 ]; then
    pass "exactly two ledger lines after Continue (one per Semi action)"
else
    fail "expected 2 ledger lines after Continue, got $LEDGER_LINES_SEMI2"
fi

echo "--- FULL: switch to unattended, poll (bounded) for game_state=game_over ---"
key "$SESS" 51; key "$SESS" 13
sleep 1
check "$CONFIG" "supervision_mode=full" "supervision set to full"

GAME_OVER=0
for i in $(seq 1 40); do
    if grep -q "game_state=game_over" "$CONFIG"; then
        GAME_OVER=1
        break
    fi
    sleep 1
done
cp "$FRAME" "$PROOF_DIR/03_full_final.txt" 2>/dev/null
cp "$LEDGER" "$PROOF_DIR/ledger_final.txt" 2>/dev/null
cp "$CONFIG" "$PROOF_DIR/config_final.txt" 2>/dev/null

if [ "$GAME_OVER" = "1" ]; then
    pass "Full mode ran the game to completion unattended within 40s"
else
    fail "Full mode did not reach game_state=game_over within 40s"
fi

echo "--- confirm automation stopped acting after game_over (no ledger growth" \
     "over a few more seconds) ---"
LINES_AT_OVER=$(wc -l < "$LEDGER" 2>/dev/null || echo 0)
sleep 3
LINES_AFTER_WAIT=$(wc -l < "$LEDGER" 2>/dev/null || echo 0)
if [ "$LINES_AT_OVER" = "$LINES_AFTER_WAIT" ]; then
    pass "ledger stopped growing after game_over ($LINES_AT_OVER lines, unchanged)"
else
    fail "ledger kept growing after game_over ($LINES_AT_OVER -> $LINES_AFTER_WAIT) - automation did not stop"
fi

echo "--- CPU sanity check across the whole Full-mode run (Pitfall 22/51) ---"
KB_PID=$(pgrep -f "$SESS/system/keyboard_input\|system/keyboard_input" | head -1)
if [ -n "$KB_PID" ]; then
    CPU=$(ps -o %cpu= -p "$KB_PID" 2>/dev/null | tr -d ' ')
    if [ -n "$CPU" ] && awk "BEGIN{exit !($CPU < 20)}"; then
        pass "keyboard_input CPU usage low ($CPU%) even after a full unattended Full-mode run"
    else
        fail "keyboard_input CPU usage high ($CPU%) - possible busy-spin regression"
    fi
else
    echo "(keyboard_input PID not found for CPU check - non-fatal, skipping)"
fi
PRISC_PID=$(pgrep -f "prisc\+x" | head -1)
if [ -n "$PRISC_PID" ]; then
    CPU2=$(ps -o %cpu= -p "$PRISC_PID" 2>/dev/null | tr -d ' ')
    if [ -n "$CPU2" ] && awk "BEGIN{exit !($CPU2 < 20)}"; then
        pass "prisc+x (the automation driver itself) CPU usage low ($CPU2%)"
    else
        fail "prisc+x CPU usage high ($CPU2%) - possible busy-spin regression in the ai_decide idle-tick"
    fi
fi

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
