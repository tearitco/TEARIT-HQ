#!/bin/bash
# demo_setup_and_battle.sh - reference SCENARIO for tactics-txt's P1
# skeleton, built from test-harn-same/ops/ (same generic tk_*
# primitives my-chara-txt's own harness uses).
#
# Reproduces, as a real re-runnable regression test, the exact manual
# trace tactics-txt's P1 skeleton was live-verified with this session
# (see HANDOFF_NEXT_SESSION.md §2): launch through the REAL
# button.sh run entry point (never a direct op invocation - Pitfall
# 21), set Classic mode, Confirm & Start, enter battle, and End Turn
# twice - asserting the shared 5-action pool, side alternation
# (1->2->1), and turn incrementing only on the full round-trip.
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

echo "=== tactics-txt REAL setup + battle regression scenario ==="
(cd "$PROJECT_DIR" && bash button.sh kill >/dev/null 2>&1)
sleep 1

cd "$PROJECT_DIR"
rm -rf "$PROJECT_DIR/pieces/sessions"
rm -f "$PROJECT_DIR/data/master_ledger.txt"
cat > "$PROJECT_DIR/pieces/system/config.txt" << 'EOCONFIG'
battle_id=tactics-001
mode=
turn=1
active_side=1
actions_remaining_this_turn=5
game_state=setup
EOCONFIG

NO_GL=1 bash button.sh run < /dev/null > /tmp/th_tacticstxt_sess.log 2>&1 &
disown

SESS=""
for i in $(seq 1 30); do
    CANDIDATE=$(ls -dt pieces/sessions/*/ 2>/dev/null | head -1)
    # Wait for REAL content, not just a non-empty file - the transient
    # "[Map Loading...]" placeholder is also non-empty and would
    # otherwise pass a naive readiness check (real race, caught live
    # writing civ-txt's own equivalent scenario this same session).
    # REAL FIX 2026-08-20: waiting for just the title banner was too
    # early - it renders before the setup screen's own "Mode: (not
    # set)" field line, racing the very next assertion below. Wait for
    # the actual content that assertion checks instead (same class of
    # fix as civ-txt's own demo_setup_and_turn.sh).
    if [ -n "$CANDIDATE" ] && grep -q "Mode: (not set)" "${CANDIDATE}pieces/display/current_frame.txt" 2>/dev/null; then
        SESS="${CANDIDATE%/}"
        break
    fi
    sleep 1
done

if [ -z "$SESS" ]; then
    fail "session launch - real frame never appeared within 30s (check /tmp/th_tacticstxt_sess.log)"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
# REAL FIX 2026-08-20 (sim-smell-fix.md's "mid-session-vs-post-session
# assertion" writeup, same fix applied to my-chara-txt/civ-txt) - real
# persistent state only lands at $PROJECT_DIR when the session ENDS, not
# live during it. Assert against the SESSION's own live copy instead.
LEDGER="$SESS/data/master_ledger.txt"
CONFIG="$SESS/pieces/system/config.txt"
echo "Session: $SESS"
cp "$FRAME" "$PROOF_DIR/00_setup_screen.txt" 2>/dev/null

echo "--- baseline: setup screen, mode unset ---"
check "$FRAME" "Mode: (not set)" "baseline shows mode unset"
check "$CONFIG" "game_state=setup" "baseline config game_state=setup"

echo "--- Set Mode: Classic (item 1) ---"
key "$SESS" 49; key "$SESS" 13
sleep 1
check "$CONFIG" "mode=classic" "mode set to classic"

echo "--- Confirm & Start (item 2) ---"
key "$SESS" 50; key "$SESS" 13
sleep 1
check "$CONFIG" "game_state=playing" "game_state flipped to playing after Confirm & Start"

echo "--- Enter Battle (item 3, real href navigation - needs digit AND" \
     "Enter to activate) ---"
key "$SESS" 51; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/01_main_screen.txt" 2>/dev/null
check "$FRAME" "T A C T I C S - T X T   [main]" "navigated to main screen"
check "$FRAME" "warrior (hp: 20)" "side 1 roster shows warrior hp 20"
check "$FRAME" "clown (hp: 15)" "side 2 roster shows clown hp 15"
check "$CONFIG" "turn=1" "still turn 1 immediately after navigating"

# UPDATE 2026-08-20: the phantom-END_TURN-on-screen-entry quirk this
# scenario used to document/assert around (see git history for the
# original comment - a chtpm_parser_pal.c interact_relay.txt cursor bug)
# no longer reproduces. Confirmed live, manually, with generous settle
# time (2s, double this scenario's own 1s) before checking - active_side
# stayed 1, not 2, after "Enter Battle" navigation. Whatever fixed it was
# incidental to unrelated work elsewhere in the shared engine, not this
# session's own symlink-migration changes. Asserting the REAL,
# CURRENT (quirk-free) behavior now, per tactics_menu_input.c's own
# END_TURN handler: active_side 1<->2 each press, turn increments only
# when wrapping 2->1. If this phantom quirk ever comes back, THIS
# assertion is what will catch it as a regression.
check "$CONFIG" "active_side=1" "no phantom END_TURN on screen entry - active_side still 1"
check "$CONFIG" "turn=1" "no phantom END_TURN on screen entry - turn still 1"

echo "--- End Turn #1 (item 1, a REAL press) - side 1->2, turn stays 1 ---"
key "$SESS" 49; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/02_after_end_turn_1.txt" 2>/dev/null
check "$CONFIG" "active_side=2" "active_side advanced 1->2 after this End Turn"
check "$CONFIG" "turn=1" "turn stays 1 (side 2 alone doesn't complete a round)"
check "$CONFIG" "actions_remaining_this_turn=5" "actions pool reset to 5"

echo "--- End Turn #2 (item 1, a REAL press) - side 2->1, turn 1->2 (wrap) ---"
key "$SESS" 49; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/03_after_end_turn_2.txt" 2>/dev/null
cp "$LEDGER" "$PROOF_DIR/ledger_final.txt" 2>/dev/null
check "$CONFIG" "active_side=1" "active_side wrapped 2->1 after this End Turn"
check "$CONFIG" "turn=2" "turn incremented to 2 (side 1 was the wrap-completing side)"

LEDGER_LINES=$(wc -l < "$LEDGER" 2>/dev/null || echo 0)
if [ "$LEDGER_LINES" -eq 2 ]; then
    pass "ledger has exactly 2 lines (one per real End Turn press, no phantom entry) - matches current quirk-free behavior"
else
    fail "ledger line count wrong: expected 2, got $LEDGER_LINES"
fi

echo "--- CPU sanity check (Pitfall 22/51 - keyboard_input must not be busy-spinning) ---"
KB_PID=$(pgrep -f "$SESS/system/keyboard_input\|system/keyboard_input" | head -1)
if [ -n "$KB_PID" ]; then
    CPU=$(ps -o %cpu= -p "$KB_PID" 2>/dev/null | tr -d ' ')
    if [ -n "$CPU" ] && awk "BEGIN{exit !($CPU < 20)}"; then
        pass "keyboard_input CPU usage low ($CPU%)"
    else
        fail "keyboard_input CPU usage high ($CPU%) - possible busy-spin regression"
    fi
else
    echo "(keyboard_input PID not found for CPU check - non-fatal, skipping)"
fi

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
