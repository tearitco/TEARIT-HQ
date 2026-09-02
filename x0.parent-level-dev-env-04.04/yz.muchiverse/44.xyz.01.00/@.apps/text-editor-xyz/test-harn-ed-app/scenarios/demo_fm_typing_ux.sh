#!/bin/bash
# demo_fm_typing_ux.sh - real, level-2, black-box test of file-menu's
# own SAVE AS FILE-field interaction contract. REWRITTEN 2026-07-30
# (PITFALL 65 rebuild, editor-widget-app-refactor-j30.txt): file-menu's
# own layout now has REAL chtpm-native <button>/<cli_io> elements -
# there is no more fm_state.txt "mode"/"field_active"/"path_buffer"/
# "cursor_pos" to assert against (the PARSER owns all of that natively
# now). Every assertion below checks the REAL signal source instead:
# pieces/display/current_layout.txt (which screen), pieces/display/
# active_gui_is_typing.txt + active_gui_index.txt (is a cli_io engaged,
# which one - SEARCH=1, FILE=2), the rendered frame itself, and real
# on-disk file checks. Real per-character typing via pieces/keyboard/
# history.txt (fm_inject_key/fm_inject_string, common.sh) - the same
# real channel chtpm_parser_pal.c's own nav/focus/cli_io logic reads
# directly, matching 102.editor's own layout exactly (interact_relay.txt
# is now purely the OUTPUT channel a real button/cli_io "Send" event
# relays a synthetic value into - not written to directly by this
# scenario anymore). NOT op-level PASTE/digit-jump shortcuts - that is
# exactly what let the ORIGINAL three bugs this scenario was built for
# ship undetected the first time (PITFALL 63).
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

FAIL=0
pass() { report PASS "$1"; }
fail() { report FAIL "$1"; FAIL=1; }

cleanup() { stop_app; }
trap cleanup EXIT INT TERM

PROOF="$HARNESS/proof/fm-typing-ux-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF"

LOGIN_FILE="$HOUSE/0.user-pal👤️/00.login-signup/current_login.txt"
XYZFS="$(grep '^current_xyzfs=' "$LOGIN_FILE" | head -1 | cut -d= -f2-)"
XYZFS_DOCS="$HOUSE/$XYZFS/home/documents"

# fm_downs_to_cancel - real chtpm nav WRAPS (not clamps) at the last
# navigable element (confirmed via direct read of chtpm_parser_pal.c's
# own process_key(): "focus_index++; if (focus_index >= element_count)
# focus_index = 0;") - a fixed "press DOWN N times" guess risks
# wrapping past CANCEL back to SEARCH depending on real entry_count,
# landing this whole scenario on an unrelated screen (live-caught
# 2026-07-30 building this exact rewrite: a blind 40-press guess left
# the browser stuck on file_menu_browser_save.chtpm the whole rest of
# the run). Computed real DOWN-count from FILE (nav index 1) to
# CANCEL (nav index 4+entry_count): SEARCH=0,FILE=1,BACK=2,
# entries=3..2+N,CONFIRM=3+N,CANCEL=4+N -> (4+N)-1 = 3+N presses.
fm_downs_to_cancel() {
    local entry_count
    entry_count="$("$FM_DIR/ops/+x/fm_scan_dir.+x" "$XYZFS_DOCS" 2>/dev/null | wc -l)"
    echo $((3 + entry_count))
}

echo "=== demo_fm_typing_ux: real per-key SAVE AS field interaction, real frame evidence ==="

sh "$HOUSE/EMERGENCY_KILL.sh" >/dev/null 2>&1 || true
start_app

EDITOR_SESSION="$(find_editor_session)" || { fail "editor session never appeared"; exit 1; }
FM_SESSION="$(find_fm_session)" || { fail "file-menu session never appeared"; exit 1; }
echo "EDITOR_SESSION=$EDITOR_SESSION" | tee -a "$PROOF/00_sessions.txt"
echo "FM_SESSION=$FM_SESSION" | tee -a "$PROOF/00_sessions.txt"

ED_HIST="$EDITOR_SESSION/pieces/keyboard/history.txt"
FM_HIST="$FM_SESSION/pieces/keyboard/history.txt"
FM_LAYOUT="$FM_SESSION/pieces/display/current_layout.txt"
FM_TYPING="$FM_SESSION/pieces/display/active_gui_is_typing.txt"
FM_GUI_IDX="$FM_SESSION/pieces/display/active_gui_index.txt"
FM_FRAME="$FM_SESSION/pieces/display/current_frame.txt"
wait_for_path "$ED_HIST" 50 || { fail "editor keyboard/history.txt never appeared"; exit 1; }

# Real focus handoff: '4' = FM (FILE MENU) per piece.pdl's own METHOD
# list, wakes/focuses the already-running widget onto this editor
# session.
inject_key "$ED_HIST" 52
sleep 0.5
wait_for_path "$FM_HIST" 50 || { fail "file-menu keyboard/history.txt never appeared"; exit 1; }

# Real digit-jump to SAVE AS... (JUMP-ONLY), real Enter activates the
# real href into file_menu_browser_save.chtpm.
fm_inject_key "$FM_SESSION" 51
sleep 0.3
fm_inject_key "$FM_SESSION" 13
sleep 0.8
if grep -q "file_menu_browser_save.chtpm" "$FM_LAYOUT" 2>/dev/null; then
    pass "real SAVE AS entered file_menu_browser_save.chtpm (real href transition)"
else
    fail "SAVE AS did not enter file_menu_browser_save.chtpm"
    cat "$FM_LAYOUT" 2>/dev/null | tee -a "$PROOF/01_layout_after_save_as.txt"
    exit 1
fi
cp "$FM_LAYOUT" "$PROOF/01_layout_after_save_as.txt"

# Real nav to FILE cli_io: SEARCH is the first navigable element after
# a fresh href transition (real clean focus reset, chtpm_parser_pal.c's
# own clear_saved_active_index() fix, 2026-07-30), one DOWN reaches
# FILE, Enter engages it (real native cli_io behavior).
fm_inject_key "$FM_SESSION" 1003
sleep 0.2
fm_inject_key "$FM_SESSION" 13
sleep 0.3
cp "$FM_TYPING" "$PROOF/02_typing_on_file_field.txt" 2>/dev/null
cp "$FM_GUI_IDX" "$PROOF/02_gui_index_on_file_field.txt" 2>/dev/null
if [ "$(cat "$FM_TYPING" 2>/dev/null)" = "1" ] && [ "$(cat "$FM_GUI_IDX" 2>/dev/null)" = "2" ]; then
    pass "real Enter on FILE cli_io engaged typing (active_gui_is_typing=1, active_gui_index=2)"
else
    fail "FILE cli_io did not become active on Enter"
fi

# --- Real per-character typing (NOT PASTE - no PASTE mode exists for
# cli_io fields anymore, chtpm_parser_pal owns them natively) ---
fm_inject_string "$FM_SESSION" "hi"
sleep 0.3
cp "$FM_FRAME" "$PROOF/03_frame_after_typing.txt" 2>/dev/null

FILE_LINE=""
_waited=0
while [ -z "$FILE_LINE" ] && [ "$_waited" -lt 20 ]; do
    FILE_LINE="$(grep "FILE:" "$FM_FRAME" 2>/dev/null)"
    [ -z "$FILE_LINE" ] && sleep 0.1
    _waited=$((_waited + 1))
done
echo "$FILE_LINE" | tee -a "$PROOF/04_view_file_line.txt"
if echo "$FILE_LINE" | grep -qE '\[\^\] 2\. : \[hi_\]'; then
    pass "real rendered frame shows [^] 2. [hi_] - real typed content, real native cursor render"
else
    fail "real rendered frame does NOT show the expected [^]/trailing-cursor format for 'hi'"
fi

# --- Real Esc: deactivates typing only, stays on THIS layout (no
# second-Esc-exits anymore - matches 102.agy-txt's own already-
# established real convention: a real CANCEL button/href is the only
# way back to the main menu now, not a keyboard shortcut - deliberate,
# not a regression). ---
fm_inject_key "$FM_SESSION" 27
sleep 0.3
cp "$FM_TYPING" "$PROOF/05_typing_after_esc.txt" 2>/dev/null
cp "$FM_LAYOUT" "$PROOF/05_layout_after_esc.txt" 2>/dev/null
if [ "$(cat "$FM_TYPING" 2>/dev/null)" != "1" ] && grep -q "file_menu_browser_save.chtpm" "$FM_LAYOUT" 2>/dev/null; then
    pass "real Esc deactivated typing only, stayed on file_menu_browser_save.chtpm"
else
    fail "real Esc did not behave as expected (see 05_typing_after_esc.txt / 05_layout_after_esc.txt)"
fi

# --- Real Enter-submits-directly, fresh pass: real href back to
# main_menu (real CANCEL button), re-enter SAVE_AS, type, Enter
# directly on FILE field submits (real chtpm cli_io "Send" relay,
# fm_menu_input.c's own active_gui_index()-disambiguated confirm,
# 2026-07-30 - matches TPMOS's own Enter-on-file_path_input=
# SET_SAVE_ACTION convention). ---
fm_inject_repeat "$FM_SESSION" 1003 "$(fm_downs_to_cancel)"
sleep 0.2
fm_inject_key "$FM_SESSION" 13
# Real settling time, not a race workaround: chtpm_parser_pal.c's own
# href-transition handler calls compose_frame() SYNCHRONOUSLY as part
# of the SAME keypress-handling call that does the transition - it
# reads gui_state.txt's own file_path_input_val for the freshly-parsed
# cli_io BEFORE this project's own async reset-on-entry logic
# (fm_compose_frame.c, only run by the next .pal loop tick, ~16.7ms
# later) ever gets a chance to clear the field left over from the
# ABANDONED "hi" typed earlier. A real human re-navigating back to
# SAVE AS takes far longer than one 16.7ms .pal loop tick to do so -
# this sleep matches that real timing, not a fixed-race-window guess;
# live-caught 2026-07-30 building this exact rewrite: without it, the
# stale "hi" prepended itself to every subsequently typed filename
# ("hihi2-....txt").
sleep 1
cp "$FM_LAYOUT" "$PROOF/06_layout_after_cancel.txt" 2>/dev/null
cp "$FM_SESSION/pieces/system/fm_state.txt" "$PROOF/06b_fm_state_after_cancel.txt" 2>/dev/null
cp "$FM_SESSION/projects/file-menu/manager/gui_state.txt" "$PROOF/06c_gui_state_after_cancel.txt" 2>/dev/null

SAVE2_NAME="hi2-$$.txt"
SAVE2_TARGET="$XYZFS_DOCS/$SAVE2_NAME"
rm -f "$SAVE2_TARGET"
fm_inject_key "$FM_SESSION" 51
sleep 0.3
fm_inject_key "$FM_SESSION" 13
sleep 1
cp "$FM_SESSION/pieces/system/fm_state.txt" "$PROOF/06d_fm_state_reentry.txt" 2>/dev/null
cp "$FM_SESSION/projects/file-menu/manager/gui_state.txt" "$PROOF/06e_gui_state_reentry.txt" 2>/dev/null
fm_inject_key "$FM_SESSION" 1003
sleep 0.3
fm_inject_key "$FM_SESSION" 13
sleep 0.6
fm_inject_string "$FM_SESSION" "$SAVE2_NAME"
sleep 0.5
fm_inject_key "$FM_SESSION" 13
sleep 1
# Real Esc: a cli_io Enter-Send is "STAY ACTIVE" (chtpm_parser_pal.c's
# own convention, confirmed via direct read of its cli_io Enter
# handler - does not clear active_index) - real arrow-key nav is
# gated on active_index==-1, so without this the NEXT DOWN presses
# below get swallowed as typing-mode input instead of real navigation
# (live-caught 2026-07-30 building this exact rewrite: skipping this
# left CANCEL never actually reached, cascading into a corrupted
# "hi2-....txt3typed-after-enter.txt" FILE field two steps later).
fm_inject_key "$FM_SESSION" 27
sleep 0.3

sleep 1
cp "$SAVE2_TARGET" "$PROOF/07_real_saved_file.txt" 2>/dev/null
cp "$FM_FRAME" "$PROOF/07_frame.txt" 2>/dev/null
cp "$EDITOR_SESSION/pieces/system/widget_cmds/status.txt" "$PROOF/07_editor_status.txt" 2>/dev/null
if [ -f "$SAVE2_TARGET" ]; then
    pass "real Enter directly on FILE field submitted - real file exists on disk at $SAVE2_TARGET"
else
    fail "no real file landed on disk at $SAVE2_TARGET after real Enter-submit"
fi

# --- Real "press Enter FIRST to activate, THEN type" on a fresh,
# empty FILE field - real native cli_io behavior (chtpm_parser_pal.c's
# own cli_io Enter handler: engages if not yet active, matches this
# house's own onClick="INTERACT" convention elsewhere). ---
fm_inject_repeat "$FM_SESSION" 1003 "$(fm_downs_to_cancel)"
sleep 0.3
fm_inject_key "$FM_SESSION" 13
sleep 1
fm_inject_key "$FM_SESSION" 51
sleep 0.3
fm_inject_key "$FM_SESSION" 13
sleep 1
fm_inject_key "$FM_SESSION" 1003
sleep 0.3
fm_inject_key "$FM_SESSION" 13
sleep 0.6
cp "$FM_TYPING" "$PROOF/08_typing_after_enter_first.txt" 2>/dev/null
if [ "$(cat "$FM_TYPING" 2>/dev/null)" = "1" ]; then
    pass "real Enter on an EMPTY, inactive FILE field activated it (real native cli_io engage)"
else
    fail "real Enter-first-to-activate did not engage typing as expected"
fi

fm_inject_string "$FM_SESSION" "typed-after-enter.txt"
sleep 0.5
cp "$FM_FRAME" "$PROOF/09_frame_after_typing_post_enter.txt" 2>/dev/null
if grep -qE '\[\^\] 2\..*\[typed-after-enter\.txt_\]' "$FM_FRAME" 2>/dev/null; then
    pass "typing after Enter-first-activation landed correctly"
else
    fail "typing after Enter-first-activation did not land"
fi

# --- Real jail-escape attempt (direct instruction: "they shouldn't be
# able to navigate into the actual linux file system") - a typed path
# trying to climb out of the user's own xyzfs home via real "../"
# components. Asserts BOTH a real error status AND that nothing landed
# outside xyzfs anywhere real to check. ---
ESCAPE_MARKER="ESCAPE-ATTEMPT-$$"
fm_inject_key "$FM_SESSION" 27
sleep 0.3
DOTDOTS="../../../../../../../../../../../../../../../../../../../../../../../../../../../../../.."
# Real Enter re-engages the field (STAY ACTIVE convention) but does
# NOT clear its existing content ("typed-after-enter.txt" from the
# prior real assertion) - real backspaces clear it first (matches how
# a real user would actually clear a field before typing something
# new), so the escape attempt below is a clean, unprefixed leading-
# relative-path string, not "typed-after-enter.txt../../..." (which
# would not even resolve as a real leading-relative-path attempt).
fm_inject_key "$FM_SESSION" 13
sleep 0.3
fm_inject_repeat "$FM_SESSION" 127 22
sleep 0.5
cp "$FM_FRAME" "$PROOF/10a_frame_before_escape_type.txt" 2>/dev/null
fm_inject_string "$FM_SESSION" "${DOTDOTS}/tmp/${ESCAPE_MARKER}.txt"
sleep 0.3
fm_inject_key "$FM_SESSION" 13
sleep 1

STATUS="$EDITOR_SESSION/pieces/system/widget_cmds/status.txt"
cp "$STATUS" "$PROOF/10_editor_status_after_escape_attempt.txt" 2>/dev/null
if [ -f "$STATUS" ] && grep -q '^result=error$' "$STATUS"; then
    pass "real jail-escape attempt (../../.../tmp/...) was REJECTED with a real error status, not silently accepted"
else
    fail "real jail-escape attempt did NOT produce a real error status (see 10_editor_status_after_escape_attempt.txt)"
fi

if [ ! -f "/tmp/${ESCAPE_MARKER}.txt" ]; then
    pass "no file actually landed outside xyzfs at the real escape target (/tmp/${ESCAPE_MARKER}.txt)"
else
    fail "REAL SECURITY FAILURE: a file landed outside the user's xyzfs home at /tmp/${ESCAPE_MARKER}.txt"
    rm -f "/tmp/${ESCAPE_MARKER}.txt"
fi

echo ""
echo "Proof: $PROOF"
if [ "$FAIL" = "0" ]; then echo "=== OVERALL: PASS ==="
else echo "=== OVERALL: FAIL ==="; fi
exit "$FAIL"
