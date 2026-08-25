#!/bin/bash
# demo_research_and_end_turn.sh - reference SCENARIO for my-biotech.
#
# SLOW BY DESIGN: unlike every sibling harness in this house, this one
# waits on a REAL, LIVE Gemma-LAN inference call (http://10.0.0.144:11434,
# gemma3:270m) - not simulated, not mocked. Real observed latency this
# session: ~45-70s per call. connect_op.+x itself allows up to 600s
# (corp_decide.c's own comment notes up to ~118s observed historically
# on this box). This scenario polls for up to 150s before declaring the
# research step failed - do not "fix" this by shortening the timeout
# without understanding why it's this generous.
#
# Reproduces the exact manual trace this project's P2 checkpoint was
# live-verified with this session (see MY_BIOTECH_DESIGN.md and
# #.haiku+/HANDOFF_NEXT_SESSION.md for the real bug found+fixed along
# the way: projects/my-biotech/pieces/mybiotech_menu/ didn't exist,
# so last_message writes silently failed even though the real gemma
# call/corpus-append/ledger-append all worked - fixed by creating that
# dir; button.sh's own "run" action now mkdir -p's it defensively).
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
check() { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }

echo "=== my-biotech REAL Research (live gemma-lan call) + End Turn scenario ==="
echo "--- pre-flight: is the LAN gemma endpoint even reachable right now? ---"
if curl -s --max-time 5 http://10.0.0.144:11434/api/tags | grep -q "gemma3:270m"; then
    pass "gemma-lan endpoint reachable, gemma3:270m listed"
else
    fail "gemma-lan endpoint NOT reachable - real research call will fail/timeout. Check the LAN address is still current (MY_BIOTECH_DESIGN.md §4 notes it is NOT guaranteed stable)."
    echo "=== ABORTING - no point running the full scenario against an unreachable endpoint ==="
    exit 1
fi

(cd "$PROJECT_DIR" && bash button.sh kill >/dev/null 2>&1)
sleep 1

cd "$PROJECT_DIR"
rm -rf "$PROJECT_DIR/pieces/sessions"
rm -f "$PROJECT_DIR/data/master_ledger.txt" "$PROJECT_DIR/data/corpus/player.txt"
mkdir -p "$PROJECT_DIR/data/corpus"
cat > "$PROJECT_DIR/pieces/system/config.txt" << 'EOCONFIG'
game_id=my-biotech-001
player_name=Adam
day=1
max_days=10
health=100
money=500
game_state=playing
EOCONFIG
rm -f "$PROJECT_DIR/projects/my-biotech/pieces/mybiotech_menu/state.txt"

# setsid: run the session in its OWN process group so the cleanup
# sweep can't take THIS scenario down with it (same fix as qtc's
# demo_signup_login_wallet.sh step-8 hang).
setsid bash button.sh run < /dev/null > /tmp/th_mybiotech_sess.log 2>&1 &
disown

SESS=""
for i in $(seq 1 30); do
    CANDIDATE=$(ls -dt pieces/sessions/*/ 2>/dev/null | head -1)
    # REAL, LIVE-CAUGHT RACE (2026-08-02): checking only that
    # current_frame.txt EXISTS can grab chtpm_parser_pal's own
    # placeholder frame ("[Map Loading...]") before main_module.pal's
    # own FIRST real mybiotech_compose_frame call has populated
    # ${game_map} - wait for the real rendered content ("Corpus:",
    # which only appears once compose_frame has actually run) instead.
    if [ -n "$CANDIDATE" ] && grep -q "Corpus:" "${CANDIDATE}pieces/display/current_frame.txt" 2>/dev/null; then
        SESS="${CANDIDATE%/}"
        break
    fi
    sleep 1
done
if [ -z "$SESS" ]; then
    fail "session launch - a real rendered frame (containing 'Corpus:') never appeared within 30s"
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
# REAL FIX 2026-08-20 (sim-smell-fix.md's "mid-session-vs-post-session
# assertion" writeup) - same fix as my-chara-txt's own demo_end_turn.sh:
# real persistent state only lands at $PROJECT_DIR when the session ENDS,
# not live during it. Assert against the SESSION's own live copy instead.
LEDGER="$SESS/data/master_ledger.txt"
CORPUS="$SESS/data/corpus/player.txt"
echo "Session: $SESS"
cp "$FRAME" "$PROOF_DIR/01_baseline.txt" 2>/dev/null
check "$FRAME" "Corpus: 0 facts" "baseline corpus is empty"

echo "--- real keystroke: Enter on 'Research' (item 1, default-selected) ---"
echo "    THIS WILL BLOCK FOR A REAL LLM CALL - polling up to 150s ---"
key "$SESS" 13

# Wait for the FULL FSM to finish (hypothesize + 4 enrich calls +
# fda_review = up to 6 real gemma calls, P3 - more than P2's single
# call), not just "corpus has one line" (which only proves HYPOTHESIZE
# ran). research_status.txt's own running=0 is the real "truly done"
# signal - it lives at data/research_status.txt (project root, NOT
# pieces/system/ - see mybiotech_menu_input.c's own header comment for
# why: session-scoped would delete this out from under a still-running
# worker if the player quits mid-research).
STATUS="$PROJECT_DIR/data/research_status.txt"
RESEARCHED=0
for i in $(seq 1 40); do
    if [ -f "$STATUS" ] && grep -q "^running=0$" "$STATUS" 2>/dev/null && [ -s "$CORPUS" ]; then
        RESEARCHED=1
        break
    fi
    sleep 5
done

if [ "$RESEARCHED" = "1" ]; then
    pass "real gemma-lan FSM completed within timeout (research_status.txt running=0), corpus grew"
else
    fail "FSM did not reach running=0 within 200s - real gemma call(s) did not complete (network issue, or connect_op.+x/json_parser.+x/mybiotech_research_worker.c regressed)"
fi

sleep 2
cp "$FRAME" "$PROOF_DIR/02_after_research.txt" 2>/dev/null
cp "$CORPUS" "$PROOF_DIR/corpus_snapshot.txt" 2>/dev/null
cp "$LEDGER" "$PROOF_DIR/ledger_snapshot.txt" 2>/dev/null

check "$LEDGER" "research_attempt" "ledger has a real research_attempt line"
# "-> discovered" is the P3 message format (2026-08-02 FSM rewrite -
# was "-> gemma-lan says: <answer>" in P2, before the full
# hypothesize->enrich->fda_review FSM existed). Update this substring
# again if mybiotech_research_worker.c's own message format changes.
check "$FRAME" "-> discovered" "frame shows the real gemma research result (regression guard for the mybiotech_menu/ dir bug)"

if grep -q "no research attempted" "$FRAME" 2>/dev/null; then
    fail "frame still says 'no research attempted' - the mybiotech_menu/ directory bug may have regressed (last_message write silently failing)"
else
    pass "frame correctly shows the research result, not the stale placeholder"
fi

echo "--- P3 regression guard: real per-compound dossier.txt + FDA verdict ---"
DOSSIER=$(find "$PROJECT_DIR/data/research" -name dossier.txt 2>/dev/null | head -1)
if [ -n "$DOSSIER" ] && [ -f "$DOSSIER" ]; then
    pass "a real dossier.txt was created under data/research/<compound>/"
    cp "$DOSSIER" "$PROOF_DIR/dossier_snapshot.txt" 2>/dev/null
    check "$DOSSIER" "[FDA Verdict]" "dossier contains a real FDA verdict line"
    check "$LEDGER" "compound_discovered" "ledger has a real compound_discovered line"
    check "$PROJECT_DIR/data/discovered_compounds.txt" "|" "discovered_compounds.txt has a real catalog entry"
else
    fail "no dossier.txt found under data/research/ - the P3 ENRICH/dossier-writing upgrade may have regressed"
fi

echo "--- real keystroke: navigate to End Turn (item 2), Enter ---"
key "$SESS" 115
key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/03_after_end_turn.txt" 2>/dev/null
check "$FRAME" "Day 2 / 10" "day advanced after End Turn"

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
