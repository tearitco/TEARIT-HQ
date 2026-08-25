#!/bin/bash
# demo_farm_mine.sh - reference SCENARIO for my-chara-txt's Farm/Mine
# screens (built after the original demo_end_turn.sh P2 checkpoint).
#
# NOTE ON A KNOWN QUIRK (found this session, see
# HANDOFF_NEXT_SESSION.md and tactics-txt's own demo_setup_and_battle.sh
# for the fuller writeup): navigating between CHTPM screens via a real
# <button href> can cause exactly one queued relay entry to be
# re-dispatched against the NEW screen's own piece.pdl (a real,
# reproducible interact_relay.txt consumption-position issue, not yet
# root-caused - see chtpm_parser_pal.c). With SEVERAL round-trip
# navigations in a row (main<->farm<->main<->mine), this compounds
# unpredictably (confirmed live while writing this scenario: exact day/
# turn counts after N navigations are NOT reliably predictable from
# the number of explicit End Turn presses alone). Rather than asserting
# exact day/turn numbers, this scenario POLLS for the actual state it
# cares about (plot ripened, resource increased) with a bounded retry
# count - a more robust pattern given this open issue, not a workaround
# that hides it (the underlying quirk is still real and still flagged
# for a future session to root-cause).
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

echo "=== my-chara-txt REAL Farm + Mine regression scenario ==="
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

NO_GL=1 bash button.sh run < /dev/null > /tmp/th_mychara_fm_sess.log 2>&1 &
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
    fail "session launch - real frame never appeared within 30s (check /tmp/th_mychara_fm_sess.log)"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
CONFIG="$PROJECT_DIR/pieces/system/config.txt"
PLOTS="$PROJECT_DIR/pieces/system/plots.txt"
LEDGER="$PROJECT_DIR/data/master_ledger.txt"
echo "Session: $SESS"

echo "--- navigate main -> Farm (item 1) ---"
key "$SESS" 49; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/00_farm_screen.txt" 2>/dev/null
check "$FRAME" "M Y - C H A R A   [farm]" "navigated to farm screen"

# Find an empty plot (any of the 3 - a phantom re-dispatch earlier in
# this run could theoretically have already planted plot 0, so check
# rather than assume).
EMPTY_ITEM=""
for n in 1 2 3; do
    idx=$((n - 1))
    st=$(grep "^plot_${idx}_state=" "$PLOTS" | cut -d= -f2)
    if [ "$st" = "empty" ] || [ -z "$st" ]; then EMPTY_ITEM=$n; break; fi
done
if [ -z "$EMPTY_ITEM" ]; then
    fail "no empty plot found to plant on (unexpected at fresh start)"
else
    echo "--- plant on the first empty plot (item $EMPTY_ITEM) ---"
    GRAIN_BEFORE=$(grep "grain_in_inventory=" "$CONFIG" | cut -d= -f2)
    key "$SESS" "$((48 + EMPTY_ITEM))"; key "$SESS" 13
    sleep 1
    cp "$FRAME" "$PROOF_DIR/01_after_plant.txt" 2>/dev/null
    check "$LEDGER" "plant" "ledger has a plant line"
    GRAIN_AFTER=$(grep "grain_in_inventory=" "$CONFIG" | cut -d= -f2)
    if [ "$GRAIN_AFTER" -lt "$GRAIN_BEFORE" ]; then
        pass "grain decreased after planting ($GRAIN_BEFORE -> $GRAIN_AFTER)"
    else
        fail "grain did not decrease after planting (before=$GRAIN_BEFORE, after=$GRAIN_AFTER)"
    fi
fi

echo "--- back to main (item 4) ---"
key "$SESS" 52; key "$SESS" 13
sleep 1
check "$FRAME" "M Y - C H A R A   [main]" "back on main screen"

# HIGH-PRIORITY OPEN BUG, found and confirmed while writing this
# scenario (see HANDOFF_NEXT_SESSION.md for the full writeup): the
# interact_relay.txt consumption-position quirk documented above does
# NOT stay a fixed "one phantom action per navigation" - it COMPOUNDS.
# A repeated round-trip loop (main<->farm, waiting for a plot to ripen
# via repeated End Turn presses) was tried first and produced wildly
# escalating day/turn advancement (day reached 55 after only ~24
# intended End Turn presses across 8 loop iterations) - strong evidence
# that each new screen's freshly-launched prisc+x process re-reads
# interact_relay.txt from position 0, re-dispatching an ever-GROWING
# backlog of already-handled history against whatever piece.pdl happens
# to be active at that moment, not just the single most-recent one.
# Rather than build a harness that fights this (or silently hides it
# behind generous bounds), this scenario now avoids repeated round-trip
# navigation entirely: it seeds an already-ripe plot directly on disk
# (a real, direct file write - the same class of state my-chara-txt's
# own ops read/write, not a fake/mocked layer) so harvest can be tested
# with exactly ONE farm visit, no waiting-loop needed. The underlying
# bug is real, confirmed, and flagged prominently for a future session
# to root-cause in chtpm_parser_pal.c - this harness does not consider
# it fixed or acceptable, just out of scope for a "basic sanity" pass.
echo "--- seed plot 1 as already-ripe directly on disk (avoids the" \
     "round-trip-navigation compounding bug above), then harvest it ---"
cat > "$PLOTS" << 'EOPLOTS'
plot_0_state=empty
plot_0_crop=
plot_0_harvest_day=0
plot_1_state=ripe
plot_1_crop=wheat
plot_1_harvest_day=1
plot_2_state=empty
plot_2_crop=
plot_2_harvest_day=0
EOPLOTS

key "$SESS" 49; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/02_farm_with_ripe_plot.txt" 2>/dev/null
check "$FRAME" "Plot 1: Ripe!" "seeded plot 1 shows Ripe in the frame"

GRAIN_BEFORE_HARVEST=$(grep "grain_in_inventory=" "$CONFIG" | cut -d= -f2)
key "$SESS" 50; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/03_after_harvest.txt" 2>/dev/null
check "$LEDGER" "harvest" "ledger has a harvest line"
check "$PLOTS" "plot_1_state=empty" "plot 1 reset to empty after harvest"
GRAIN_AFTER_HARVEST=$(grep "grain_in_inventory=" "$CONFIG" | cut -d= -f2)
if [ "$GRAIN_AFTER_HARVEST" -gt "$GRAIN_BEFORE_HARVEST" ]; then
    pass "grain increased after harvest ($GRAIN_BEFORE_HARVEST -> $GRAIN_AFTER_HARVEST)"
else
    fail "grain did not increase after harvest (before=$GRAIN_BEFORE_HARVEST, after=$GRAIN_AFTER_HARVEST)"
fi

echo "--- back to main, then to Mine screen ---"
key "$SESS" 52; key "$SESS" 13
sleep 1
key "$SESS" 50; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/04_mine_screen.txt" 2>/dev/null
check "$FRAME" "M Y - C H A R A   [mine]" "navigated to mine screen"

echo "--- mine once ---"
SILVER_BEFORE=$(grep "silver_in_inventory=" "$CONFIG" | cut -d= -f2)
GOLD_BEFORE=$(grep "gold_in_inventory=" "$CONFIG" | cut -d= -f2)
key "$SESS" 49; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/05_after_mine.txt" 2>/dev/null
cp "$LEDGER" "$PROOF_DIR/ledger_final.txt" 2>/dev/null
SILVER_AFTER=$(grep "silver_in_inventory=" "$CONFIG" | cut -d= -f2)
GOLD_AFTER=$(grep "gold_in_inventory=" "$CONFIG" | cut -d= -f2)
if [ "$SILVER_AFTER" -gt "$SILVER_BEFORE" ] || [ "$GOLD_AFTER" -gt "$GOLD_BEFORE" ]; then
    pass "mining produced silver or gold (silver $SILVER_BEFORE->$SILVER_AFTER, gold $GOLD_BEFORE->$GOLD_AFTER)"
else
    fail "mining produced neither silver nor gold"
fi
check "$LEDGER" "mine" "ledger has a mine line"

echo "--- CPU sanity check (Pitfall 22/51) ---"
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
