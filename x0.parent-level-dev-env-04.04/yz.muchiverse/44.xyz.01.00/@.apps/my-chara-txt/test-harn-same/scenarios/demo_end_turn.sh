#!/bin/bash
# demo_end_turn.sh - reference SCENARIO for my-chara-txt, built from
# test-harn-same/ops/ (same generic tk_* primitives 045.muchi-pal-agent's
# own harness uses - see that project's own demo_list_dir_tool.sh, this
# scenario is modeled directly on its shape).
#
# Reproduces, as a real re-runnable regression test, the exact manual
# trace this project's P2 checkpoint was live-verified with (see
# #.haiku+/HANDOFF_NEXT_SESSION.md §6.2): launch through the REAL
# button.sh run entry point (never a direct op invocation - Pitfall 21),
# inject two real Enter keypresses (the only menu item on main.chtpm,
# "End Turn", is already the default cursor position), and assert both
# the ledger and config.txt actually mutated - not simulated, not
# op-level-only.
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

echo "=== my-chara-txt REAL End Turn regression scenario ==="
(cd "$PROJECT_DIR" && bash button.sh kill >/dev/null 2>&1)
sleep 1

cd "$PROJECT_DIR"
# Same real, live-caught race class documented in 045's own
# demo_list_dir_tool.sh: clear any stale session dir ourselves BEFORE
# launching so a readiness-poll can't lock onto a dead directory.
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
EOCONFIG

bash button.sh run < /dev/null > /tmp/th_mychara_sess.log 2>&1 &
disown

SESS=""
for i in $(seq 1 30); do
    CANDIDATE=$(ls -dt pieces/sessions/*/ 2>/dev/null | head -1)
    # REAL RACE, CAUGHT LIVE (2026-08-02, writing civ-txt/tactics-txt's
    # own equivalent scenarios this same session): a bare "-f
    # current_frame.txt" check is satisfied by chtpm_parser_pal's own
    # transient "[Map Loading...]" placeholder frame too, not just a
    # real compose_frame render - grep for actual screen content, or
    # this readiness check can spuriously succeed against a frame
    # that's about to be replaced a moment later.
    # REAL FIX 2026-08-20: waiting for just the title banner was too
    # early on a fast back-to-back rerun - it can render before the
    # "Day 1 / 10" content line, racing the very next assertion below
    # (same class of fix as civ-txt/tactics-txt's own scenarios). Wait
    # for the actual content that assertion checks instead.
    if [ -n "$CANDIDATE" ] && grep -q "Day 1 / 10" "${CANDIDATE}pieces/display/current_frame.txt" 2>/dev/null; then
        SESS="${CANDIDATE%/}"
        break
    fi
    sleep 1
done

if [ -z "$SESS" ]; then
    fail "session launch - current_frame.txt never appeared within 30s (check /tmp/th_mychara_sess.log)"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
# REAL FIX 2026-08-20 (sim-smell-fix.md's own "mid-session-vs-post-session
# assertion" writeup): under the copy-based symlink-elimination strategy,
# PRISC_PROJECT_ROOT stays session-scoped and real persistent state
# (config.txt, data/) only gets copied back to $PROJECT_DIR by
# persist_session_state() when the SESSION ENDS, not live during it. This
# scenario asserts mid-session, so it must read the SESSION's own live
# copy, not the (stale-until-exit) real-root copy - was $PROJECT_DIR/...,
# which made every mid-session check here false-fail even on a fully
# correct run (confirmed live: final config.txt at $PROJECT_DIR was
# correct - day=3, health=90 - after the run finished; only the
# mid-session reads were wrong).
LEDGER="$SESS/data/master_ledger.txt"
CONFIG="$SESS/pieces/system/config.txt"
echo "Session: $SESS"
cp "$FRAME" "$PROOF_DIR/before_end_turn.txt" 2>/dev/null

echo "--- baseline: day=1, health=100, empty ledger ---"
check "$FRAME" "Day 1 / 10" "baseline frame shows Day 1"
check "$CONFIG" "day=1" "baseline config day=1"

echo "--- real keystroke #1: select End Turn (item 4 - no longer the" \
     "only/default item since Farm/Mine/Automation buttons were added" \
     "to main.chtpm later this session, digit+Enter now required" \
     "rather than a bare Enter) ---"
key "$SESS" 52; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/after_turn_1.txt"

check "$LEDGER" "day_end" "ledger has a day_end line after turn 1"
check "$CONFIG" "day=2" "config day advanced to 2 after turn 1"
check "$CONFIG" "health=95" "config health decayed to 95 after turn 1"
check "$FRAME" "Day 2 / 10" "frame re-rendered showing Day 2 after turn 1"

echo "--- real keystroke #2: End Turn again (repeatability/stability check) ---"
key "$SESS" 52; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/after_turn_2.txt"
cp "$LEDGER" "$PROOF_DIR/ledger_final.txt" 2>/dev/null

check "$CONFIG" "day=3" "config day advanced to 3 after turn 2"
check "$CONFIG" "health=90" "config health decayed to 90 after turn 2"
check "$FRAME" "Day 3 / 10" "frame re-rendered showing Day 3 after turn 2"

LEDGER_LINES=$(wc -l < "$LEDGER" 2>/dev/null || echo 0)
if [ "$LEDGER_LINES" -eq 2 ]; then
    pass "ledger is append-only with exactly 2 lines (one per real turn, none lost/duplicated)"
else
    fail "ledger line count wrong: expected 2, got $LEDGER_LINES"
fi

echo "--- CPU sanity check (Pitfall 22/51 - keyboard_input must not be busy-spinning) ---"
KB_PID=$(pgrep -f "$SESS/system/keyboard_input\|system/keyboard_input" | head -1)
if [ -n "$KB_PID" ]; then
    CPU=$(ps -o %cpu= -p "$KB_PID" 2>/dev/null | tr -d ' ')
    if [ -n "$CPU" ] && awk "BEGIN{exit !($CPU < 20)}"; then
        pass "keyboard_input CPU usage low ($CPU%) - Pitfall 51 fix holding"
    else
        fail "keyboard_input CPU usage high ($CPU%) - Pitfall 51 fix may have regressed"
    fi
else
    echo "(keyboard_input PID not found for CPU check - non-fatal, skipping)"
fi

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
