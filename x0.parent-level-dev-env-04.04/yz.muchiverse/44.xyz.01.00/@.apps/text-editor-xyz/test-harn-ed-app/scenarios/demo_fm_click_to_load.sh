#!/bin/bash
# demo_fm_click_to_load.sh - real, level-2, black-box test of the TWO
# interaction paths found ONLY by live human testing, never by any
# harness before this one (see !.xyzos-standards+1.txt §38's own
# addendum and PITFALL 63's own addendum, same date, for the general
# lesson this scenario exists to guard against recurring):
#
#   1) selecting a REAL LISTED FILE ENTRY from the directory browser
#      (arrow-nav to it, one real Enter) must load it immediately -
#      every prior scenario (demo_load.sh, demo_fm_typing_ux.sh) only
#      ever TYPED or PASTED a path directly into the FILE field, never
#      once selected an entry from the real rendered listing. This is
#      arguably the MORE natural way a real user picks a file (click
#      what you see) and it was silently broken (one Enter only
#      pre-filled the field, requiring a second Enter no UI affordance
#      hinted at) until a human found it live.
#   2) a digit keypress on the main menu (main_menu mode) must ONLY
#      move the cursor - a SEPARATE real Enter is required to actually
#      activate the selected option. Every prior scenario's own digit-
#      press was always immediately followed by a real Enter anyway
#      (needed once this was fixed), so none of them could tell the
#      difference between "digit jumps only" (the real, now-fixed
#      standard) and "digit jumps AND activates in one keystroke" (the
#      old, wrong behavior) - this scenario asserts the STATE
#      immediately after the digit alone, before any Enter, to close
#      that gap for real.
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

FAIL=0
pass() { report PASS "$1"; }
fail() { report FAIL "$1"; FAIL=1; }

cleanup() { stop_app; }
trap cleanup EXIT INT TERM

PROOF="$HARNESS/proof/fm-click-to-load-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF"

echo "=== demo_fm_click_to_load: real listing-selection + real digit-jump-only, real key injection ==="

LOGIN_FILE="$HOUSE/0.user-pal👤️/00.login-signup/current_login.txt"
XYZFS="$(grep '^current_xyzfs=' "$LOGIN_FILE" | head -1 | cut -d= -f2-)"
XYZFS_DOCS="$HOUSE/$XYZFS/home/documents"
mkdir -p "$XYZFS_DOCS"

# Unique fixture, filtered to via SEARCH so its listing INDEX is
# deterministic (always the first, and only, real entry - index 3)
# regardless of whatever else is already in the user's real documents/
# folder - a harness must not be fragile to the user's own real files
# sitting alongside its fixtures.
MARKER="CLICK-TO-LOAD-MARKER-$$-$(date +%s)"
FIXTURE_NAME="click-load-fixture-$$.txt"
FIXTURE="$XYZFS_DOCS/$FIXTURE_NAME"
printf '%s\n' "$MARKER" > "$FIXTURE"
cp "$FIXTURE" "$PROOF/00_fixture.txt"

sh "$HOUSE/EMERGENCY_KILL.sh" >/dev/null 2>&1 || true
start_app

EDITOR_SESSION="$(find_editor_session)" || { fail "editor session never appeared"; exit 1; }
FM_SESSION="$(find_fm_session)" || { fail "file-menu session never appeared"; exit 1; }
echo "EDITOR_SESSION=$EDITOR_SESSION" | tee -a "$PROOF/01_sessions.txt"
echo "FM_SESSION=$FM_SESSION" | tee -a "$PROOF/01_sessions.txt"

ED_HIST="$EDITOR_SESSION/pieces/keyboard/history.txt"
FM_STATE="$FM_SESSION/pieces/system/fm_state.txt"
wait_for_path "$ED_HIST" 50 || { fail "editor keyboard/history.txt never appeared"; exit 1; }

inject_key "$ED_HIST" 52
sleep 0.5
wait_for_path "$FM_SESSION/pieces/apps/player_app/interact_relay.txt" 50 \
    || { fail "file-menu interact_relay.txt never appeared"; exit 1; }

# --- Part 1: digit alone must NOT activate (real standard, fixed
# 2026-07-30 - see fm_menu_input.c's own handle_main_menu() comment) ---
fm_relay_key "$FM_SESSION" 52   # '4' = LOAD, digit only, NO Enter yet
sleep 0.3
cp "$FM_STATE" "$PROOF/02_fm_state_after_digit_only.txt"
if grep -q "^mode=main_menu$" "$FM_STATE" 2>/dev/null && grep -q "^cursor_pos=4$" "$FM_STATE" 2>/dev/null; then
    pass "digit press alone moved cursor to LOAD but did NOT activate it (still main_menu) - real standard, not assumed"
else
    fail "digit press alone did not behave as jump-only (see 02_fm_state_after_digit_only.txt)"
fi

# NOW the real, separate Enter that actually activates it.
fm_relay_key "$FM_SESSION" 13
sleep 0.3
cp "$FM_STATE" "$PROOF/03_fm_state_after_enter.txt"
if grep -q "^mode=file_browser$" "$FM_STATE" 2>/dev/null && grep -q "^browse_mode=load$" "$FM_STATE" 2>/dev/null; then
    pass "real, separate Enter activated LOAD after the digit-only jump"
else
    fail "real Enter did not activate LOAD after the digit-only jump"
fi

# --- Part 2: SEARCH-filter down to the one fixture, then select it
# from the real listing by arrow-nav + one real Enter (NOT typing/
# pasting its path) - the exact interaction that was broken live. ---
fm_paste "$FM_SESSION" "$FIXTURE_NAME"   # types into SEARCH (cursor_pos=0 by default on fresh entry)
sleep 0.3
cp "$FM_STATE" "$PROOF/04_fm_state_after_search.txt"

# cursor_pos 0=SEARCH,1=FILE,2=BACK,3=first (only, now filtered) entry.
# Two real ARROW_DOWN presses: SEARCH -> FILE -> BACK -> entry.
fm_relay_key "$FM_SESSION" 1003
sleep 0.2
fm_relay_key "$FM_SESSION" 1003
sleep 0.2
fm_relay_key "$FM_SESSION" 1003
sleep 0.2
cp "$FM_STATE" "$PROOF/05_fm_state_on_entry.txt"
if grep -q "^cursor_pos=3$" "$FM_STATE" 2>/dev/null; then
    pass "real arrow navigation reached the filtered listing's own entry (cursor_pos=3)"
else
    fail "arrow navigation did not reach the expected entry index (see 05_fm_state_on_entry.txt)"
fi

# ONE real Enter on the listed entry itself - this is the exact real
# human action that used to require a SECOND Enter (on the FILE field)
# before this fix, with no visible reason why to a real user.
fm_relay_key "$FM_SESSION" 13
sleep 1

BUFFER="$EDITOR_SESSION/pieces/system/editor_buffer.txt"
FRAME="$EDITOR_SESSION/pieces/display/current_frame.txt"
cp "$BUFFER" "$PROOF/06_editor_buffer_after_click_load.txt" 2>/dev/null
cp "$FRAME" "$PROOF/06_current_frame_after_click_load.txt" 2>/dev/null

if [ -f "$BUFFER" ] && grep -qF "$MARKER" "$BUFFER"; then
    pass "ONE real Enter on a listed file entry loaded it directly into the editor buffer - real click-to-load, not a typed path"
else
    fail "editor_buffer.txt does not contain the marker after selecting the listed entry - click-to-load did NOT work"
fi

if [ -f "$FRAME" ] && grep -qF "$MARKER" "$FRAME"; then
    pass "current_frame.txt (RENDERED frame) shows the loaded content - level-2 proof, not just state"
else
    fail "rendered frame does not show the loaded marker"
fi

rm -f "$FIXTURE"

echo ""
echo "Proof: $PROOF"
if [ "$FAIL" = "0" ]; then echo "=== OVERALL: PASS ==="
else echo "=== OVERALL: FAIL ==="; fi
exit "$FAIL"
