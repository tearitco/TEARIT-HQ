#!/bin/bash
# demo_setup_and_turn.sh - reference SCENARIO for civ-txt's P1
# skeleton, built from test-harn-same/ops/ (same generic tk_*
# primitives my-chara-txt's own harness uses).
#
# Reproduces, as a real re-runnable regression test, the exact manual
# trace civ-txt's P1 skeleton was live-verified with this session
# (see HANDOFF_NEXT_SESSION.md §2): launch through the REAL
# button.sh run entry point (never a direct op invocation - Pitfall
# 21), pick Victory/Map/Combat setup options, Confirm & Start, enter
# the game, and End Turn - asserting real config.txt/ledger mutation
# at every step.
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

echo "=== civ-txt REAL setup + turn regression scenario ==="
(cd "$PROJECT_DIR" && bash button.sh kill >/dev/null 2>&1)
sleep 1

cd "$PROJECT_DIR"
rm -rf "$PROJECT_DIR/pieces/sessions"
rm -f "$PROJECT_DIR/data/master_ledger.txt"
cat > "$PROJECT_DIR/pieces/system/config.txt" << 'EOCONFIG'
game_id=civ-txt-001
turn=1
turn_order_index=0
victory_condition=
map_scale=
combat_resolution=
treasury=50
city_count=1
game_state=setup
EOCONFIG

# NO_GL=1 keeps the harness headless-safe regardless of what's set in
# the caller's own environment - a test run should never depend on a
# real DISPLAY being present.
NO_GL=1 bash button.sh run < /dev/null > /tmp/th_civtxt_sess.log 2>&1 &
disown

SESS=""
for i in $(seq 1 30); do
    CANDIDATE=$(ls -dt pieces/sessions/*/ 2>/dev/null | head -1)
    # REAL RACE, CAUGHT LIVE: a bare "-s current_frame.txt" (non-empty)
    # check is satisfied by chtpm_parser_pal's own transient
    # "[Map Loading...]" placeholder frame too, not just a real
    # compose_frame render - grep for actual screen content, not just
    # file existence, or the very first assertion below can spuriously
    # fail against a frame that's about to be replaced a moment later.
    # REAL FIX 2026-08-20: waiting for just the "C I V - T X T" title
    # banner was STILL too early - that banner renders in an earlier
    # frame than the setup screen's own "Victory condition:" field line,
    # so the very next assertion below (which checks for that exact
    # string) raced against it and false-failed. Wait for the actual
    # content the first real assertion checks, not just the title.
    if [ -n "$CANDIDATE" ] && grep -q "Victory condition:" "${CANDIDATE}pieces/display/current_frame.txt" 2>/dev/null; then
        SESS="${CANDIDATE%/}"
        break
    fi
    sleep 1
done

if [ -z "$SESS" ]; then
    fail "session launch - current_frame.txt never appeared within 30s (check /tmp/th_civtxt_sess.log)"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
# REAL FIX 2026-08-20 (sim-smell-fix.md's "mid-session-vs-post-session
# assertion" writeup, same fix applied to my-chara-txt's own
# demo_end_turn.sh) - under the copy-based symlink-elimination strategy,
# real persistent state only lands at $PROJECT_DIR when the session ENDS
# (persist_session_state() in button.sh's EXIT trap), not live during it.
# Assert against the SESSION's own live copy instead.
LEDGER="$SESS/data/master_ledger.txt"
CONFIG="$SESS/pieces/system/config.txt"
echo "Session: $SESS"
cp "$FRAME" "$PROOF_DIR/00_setup_screen.txt" 2>/dev/null

echo "--- baseline: setup screen, no options picked ---"
check "$FRAME" "Victory condition: (not set)" "baseline shows victory unset"
check "$CONFIG" "game_state=setup" "baseline config game_state=setup"

echo "--- pick Victory: Conquest (item 1) ---"
key "$SESS" 49; key "$SESS" 13
sleep 1
check "$CONFIG" "victory_condition=conquest" "victory_condition set to conquest"

echo "--- pick Map: Small (item 4) ---"
key "$SESS" 52; key "$SESS" 13
sleep 1
check "$CONFIG" "map_scale=small" "map_scale set to small"

echo "--- pick Combat: Abstract (item 6) ---"
key "$SESS" 54; key "$SESS" 13
sleep 1
check "$CONFIG" "combat_resolution=abstract" "combat_resolution set to abstract"
cp "$FRAME" "$PROOF_DIR/01_setup_complete.txt" 2>/dev/null

echo "--- Confirm & Start (item 8) ---"
key "$SESS" 56; key "$SESS" 13
sleep 1
check "$CONFIG" "game_state=playing" "game_state flipped to playing after Confirm & Start"

echo "--- Enter Game (item 9, real href navigation - needs digit AND Enter" \
     "to activate, same as any other numbered nav item; confirmed live," \
     "digit alone only moves the cursor) ---"
key "$SESS" 57; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/02_main_screen.txt" 2>/dev/null
check "$FRAME" "C I V - T X T   [main]" "navigated to main screen"

echo "--- End Turn (item 1) ---"
key "$SESS" 49; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/03_after_end_turn.txt" 2>/dev/null
cp "$LEDGER" "$PROOF_DIR/ledger_final.txt" 2>/dev/null
check "$LEDGER" "end_turn" "ledger has an end_turn line"

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
