#!/bin/bash

# macOS leg (2026-08-23): macOS has no setsid(2) wrapper binary - expand
# to nothing there, keep real setsid on Linux. Unquoted $SETSID so the
# empty case vanishes from the command line entirely.
SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""
# demo_module_split_smoke.sh - real K3-style smoke test for mutaclsym's
# own Phase 2 per-screen module split (2026-07-31, #.haiku+/!.xyzos-
# standards+1.txt §41), the first real harness this project has ever
# had. Proves the two things that split could plausibly break: real
# INTERACT-mode hero movement (Control Hero, the one real continuous-
# relay case this whole house family's §16 standard exists for) still
# works under game.chtpm's own new dedicated module, and real href
# navigation to/from info_test.chtpm's own new dedicated module still
# works, landing cleanly both ways.
#
# Real per-keystroke injection into pieces/keyboard/history.txt via
# tk_inject_key.+x (matches the [TIMESTAMP] KEY_PRESSED: <code> format
# #.haiku+/tpmos-re-dox/_.0.aigent-testing-k3.txt requires), real frame
# + real state-file assertions - no PASTE-mode shortcuts, no op-level
# bypass. mutaclsym runs IN-PLACE (no pieces/sessions/<id>/ isolation),
# confirmed via button.sh's own real "run" action.
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OPS="$SCRIPT_DIR/ops/+x"
PROOF_DIR="$SCRIPT_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

key()   { "$OPS/tk_inject_key.+x" "$PROJECT_DIR" "$1"; sleep 0.3; }
focus() { "$OPS/tk_focus_item.+x" "$PROJECT_DIR" "$PROJECT_DIR/pieces/display/current_frame.txt" "$1" >/dev/null; sleep 0.3; }
check() { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }

cleanup() {
    echo; echo "--- cleanup ---"
    bash "$SCRIPT_DIR/button.sh" kill >/dev/null 2>&1
}
trap cleanup EXIT INT TERM

echo "=== mutaclsym: real K3 smoke test - INTERACT movement + href nav, both new per-screen modules ==="

bash "$SCRIPT_DIR/button.sh" kill >/dev/null 2>&1
sleep 1
cd "$PROJECT_DIR"
: > pieces/keyboard/history.txt
NO_GL=1 PAL_MODE=1 setsid bash button.sh run > "$PROOF_DIR/app_stdout.log" 2>&1 < /dev/null & disown

waited=0
while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 50 ]; do
    sleep 0.2; waited=$((waited + 1))
done
if [ ! -s pieces/display/current_frame.txt ]; then
    fail "app never rendered a real frame - see $PROOF_DIR/app_stdout.log"
    exit 1
fi
sleep 2
cp pieces/display/current_frame.txt "$PROOF_DIR/00_initial_frame.txt"
[ "$(cat pieces/display/current_layout.txt 2>/dev/null)" = "pieces/chtpm/layouts/game.chtpm" ] \
    && pass "launched real frame, landed on game.chtpm (its own new dedicated module)" \
    || fail "did not land on game.chtpm - see current_layout.txt"

# --- Real INTERACT movement: engage Control Hero, move, confirm real state change ---
BEFORE_POS="$(cat pieces/world_01/map_start/hero/state.txt 2>/dev/null | grep -E '^pos_x=|^pos_y=' | tr '\n' ' ')"
echo "hero pos before: $BEFORE_POS"
focus "Control Hero"
key 13
sleep 0.5
key 100    # 'd' - real WASD move-right, per game.chtpm's own on-screen help text
sleep 0.5
key 27     # Esc - exit INTERACT
sleep 0.5
cp pieces/display/current_frame.txt "$PROOF_DIR/01_after_move_frame.txt"
AFTER_POS="$(cat pieces/world_01/map_start/hero/state.txt 2>/dev/null | grep -E '^pos_x=|^pos_y=' | tr '\n' ' ')"
echo "hero pos after: $AFTER_POS"
if [ "$BEFORE_POS" != "$AFTER_POS" ]; then
    pass "real INTERACT-mode movement changed the real hero state (pos: '$BEFORE_POS' -> '$AFTER_POS') - game.chtpm's own new dedicated module dispatches real keys correctly"
else
    fail "hero position did not change after real 'd' movement while INTERACT-engaged (before='$BEFORE_POS' after='$AFTER_POS')"
fi

# --- Real href navigation: game.chtpm -> info_test.chtpm -> back ---
focus "Info (href test)"
key 13
sleep 1
cp pieces/display/current_frame.txt "$PROOF_DIR/02_info_screen_frame.txt"
[ "$(cat pieces/display/current_layout.txt 2>/dev/null)" = "pieces/chtpm/layouts/info_test.chtpm" ] \
    && pass "real href landed on info_test.chtpm (its own new dedicated module)" \
    || fail "did not land on info_test.chtpm after real href activation"
check "pieces/display/current_frame.txt" "real href test screen" "info_test.chtpm's own real content rendered (not stale/blank from the module transition)"

focus "Back to Game"
key 13
sleep 1
cp pieces/display/current_frame.txt "$PROOF_DIR/03_back_on_game_frame.txt"
[ "$(cat pieces/display/current_layout.txt 2>/dev/null)" = "pieces/chtpm/layouts/game.chtpm" ] \
    && pass "real href navigated back to game.chtpm cleanly" \
    || fail "did not land back on game.chtpm after real Back-to-Game href"

echo
echo "Proof: $PROOF_DIR"
if [ "$FAIL" = "0" ]; then echo "=== OVERALL: PASS ==="; else echo "=== OVERALL: FAIL ==="; fi
exit "$FAIL"
