#!/bin/bash
# demo_save_load.sh - proves 102.agy-txt's own real edit -> save -> new
# -> load loop end to end, through the REAL running app, real key
# injection (§36.6 level 2), matching test-harn-ed-app's own real
# precedent shape - not an op-level shortcut.
#
# Flow: launch -> real INTERACT typing (unique marker, not just the
# static seed buffer) -> real menu nav to SAVE AS -> real path typing
# -> real save -> assert the REAL FILE on disk -> real NEW (clears
# buffer) -> real menu nav to LOAD -> real path typing -> real load ->
# assert the buffer is restored from disk, not just "looks unchanged".
#
# NAV METHOD, REAL FINDING FROM BUILDING THIS SCRIPT (not assumed):
# this project's layouts are real CHTPM buttoned layouts, using
# chtpm_parser_pal.c's own GENERIC button nav - NOT file-menu widget's
# own custom "clamps at max_idx" nav (file-menu implements its own nav
# entirely in fm_menu_input.c, a different, buttonless architecture).
# Confirmed by direct read of chtpm_parser_pal.c: ARROW_UP/ARROW_DOWN
# WRAP AROUND cyclically (`if (focus_index < 0) focus_index =
# element_count-1;`), they do NOT clamp - "press UP enough times to
# guarantee item 1" is FALSE for this nav model and produced flaky,
# session-state-dependent failures when first tried. Real fix: use
# DIRECT DIGIT-KEY JUMP instead (chtpm_parser_pal.c's own
# `isdigit(key)` -> `do_jump()` branch - press the ASCII digit for the
# target item number, then Enter to activate it) - deterministic
# regardless of where focus started.
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HARNESS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/common.sh"

FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

cleanup() { stop_app; }
trap cleanup EXIT INT TERM

PROOF="$HARNESS_DIR/proof/save-load-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF"

echo "=== agy-txt: real edit -> save -> new -> load, via real key injection ==="

MARKER="AGY-HARNESS-MARKER-$$-$(date +%s)"
# BARE relative filename, not "docs/..." (2026-07-30, save-bug.txt's
# own fix, direct instruction, applied to agy_widget_cmds.c the same
# way as editor_widget_cmds.c - PITFALL 62's own docs/ symlink is
# superseded by the same real xyzfs-home jail every project's SAVE_AS
# now uses: a bare name resolves under <xyzfs_home>/documents/, never
# the project's own local docs/ folder). Resolve that same real
# location here for the assertion below, via the identical 2-hop chain
# resolve_save_path() itself uses.
SAVE_NAME="harness_$$.txt"
LOGIN_FILE="$HOUSE/0.user-pal👤️/00.login-signup/current_login.txt"
XYZFS="$(grep '^current_xyzfs=' "$LOGIN_FILE" | head -1 | cut -d= -f2-)"
SAVE_PATH="$SAVE_NAME"
REAL_SAVE_TARGET_XYZFS="$HOUSE/$XYZFS/home/documents/$SAVE_NAME"
rm -f "$REAL_SAVE_TARGET_XYZFS"

stop_app
start_app

SESS="$(find_session)" || { fail "session launch - current_frame.txt never appeared"; exit 1; }
echo "SESSION=$SESS" | tee "$PROOF/00_session.txt"
cp "$SESS/pieces/display/current_frame.txt" "$PROOF/01_initial_frame.txt"

# jump_to <session> <item_number> - direct digit-key nav (ASCII '1'=49),
# deterministic regardless of persisted focus state (see header note).
jump_to() {
    local session="$1" n="$2"
    inject_key "$session" "$((48 + n))"
}

# --- Real edit: type a unique marker into the editor canvas ---
jump_to "$SESS" 1                # EDIT TEXT (INTERACT)
inject_key "$SESS" 13
wait_for_typing "$SESS" 30 || { fail "INTERACT never engaged on editor.chtpm"; exit 1; }
pass "INTERACT engaged on editor canvas"
ag_paste "$SESS" " $MARKER"
sleep 0.3
inject_key "$SESS" 27            # Esc -> exit INTERACT
sleep 0.3
if grep -qF "$MARKER" "$SESS/pieces/system/editor_buffer.txt"; then
    pass "real typed marker landed in editor_buffer.txt"
else
    fail "marker never appeared in editor_buffer.txt after real typing"
fi

# --- Navigate to FILE MENU -> SAVE AS ---
jump_to "$SESS" 2                 # FILE MENU
inject_key "$SESS" 13
sleep 0.3
[ "$(current_layout_of "$SESS")" = "pieces/chtpm/layouts/file_menu.chtpm" ] \
    && pass "navigated to file_menu.chtpm" || fail "did not land on file_menu.chtpm"

jump_to "$SESS" 3                 # SAVE AS...
inject_key "$SESS" 13
sleep 0.3
[ "$(current_layout_of "$SESS")" = "pieces/chtpm/layouts/file_browser_save.chtpm" ] \
    && pass "navigated to file_browser_save.chtpm" || fail "did not land on file_browser_save.chtpm"

# --- Real path typing + real SAVE ---
# REAL FIX (2026-07-31, found while testing an unrelated per-screen
# module split - this bug pre-dates that work, confirmed by
# reproducing identically on the fully-reverted original code): this
# used to jump_to item 1 (assumed "path field"), paste, Esc, then
# jump_to item 2 (assumed "SAVE"). That's stale - direct read of the
# real current layout (file_browser_save.chtpm) and the real manager
# (agy_browser_manager.c's own active_gui_index()==2 comment) confirms
# item 1 is SEARCH, item 2 is the real FILE field, and pressing Enter
# WHILE the FILE field is actively engaged (agi==2) dispatches
# "confirm" directly - no separate SAVE button click needed, and no
# fragile "guess the dynamic item number of SAVE FILE among however
# many real directory entries exist right now" required either.
jump_to "$SESS" 2                 # FILE field (real item 2, not 1)
inject_key "$SESS" 13
wait_for_typing "$SESS" 30 || { fail "INTERACT never engaged on FILE field (save)"; exit 1; }
# REAL FIX (2026-07-31): ag_paste (agy_edit_key.+x PASTE) was built for
# the OLD editor/path-typing convention and never updated to know about
# the REBUILT browser's own real <cli_io id="file_path_input"> field -
# it silently no-ops here (FILE field stayed empty in the frame, proof
# 02_after_save_frame.txt showed "[_]" and "Type path or browse to a
# file" unchanged). Real per-keystroke typing into pieces/keyboard/
# history.txt (the same channel jump_to's own digit-nav already uses)
# is what chtpm_parser_pal.c's own real <cli_io> typing logic actually
# consumes - ported tk_type_text.c from the pal-chain family's own
# proven, project-agnostic version of this exact primitive.
"$HARNESS_DIR/ops/+x/tk_type_text.+x" "$SESS" "$SAVE_PATH"
sleep 0.3
inject_key "$SESS" 13             # real Enter-on-FILE-field confirm, not Esc+jump
sleep 1
# REAL FIX (2026-07-31): confirm() dispatches the save on the manager
# side, but does NOT exit the real <cli_io> field's own engaged/typing
# state at the chtpm_parser_pal.c level - live-caught: the very next
# digit keystroke (meant for CANCEL's own focus-jump) landed as a
# literal typed character in the still-open FILE field instead of real
# navigation, re-triggering SAVE with a garbage one-character filename.
# Explicit Esc here to really exit typing mode before any further nav.
inject_key "$SESS" 27
sleep 0.3
cp "$SESS/pieces/display/current_frame.txt" "$PROOF/02_after_save_frame.txt"

REAL_SAVE_TARGET="$REAL_SAVE_TARGET_XYZFS"
if [ -f "$REAL_SAVE_TARGET" ] && grep -qF "$MARKER" "$REAL_SAVE_TARGET"; then
    pass "real file with the real marker landed at the REAL xyzfs documents/ location ($REAL_SAVE_TARGET) — save-bug.txt's own fix"
else
    fail "no real file (with marker) found at $REAL_SAVE_TARGET after SAVE"
fi

# --- Navigate back to FILE MENU, real NEW (clears the buffer) ---
# REAL FIX (2026-07-31, same bug class as the SAVE fix above): item 3
# is the directory "<- BACK" (go up a folder), NOT this widget's own
# CANCEL-to-file_menu button - that button's real item number is
# DYNAMIC (shifts with however many real directory entries currently
# exist). Find it by real label text instead of guessing a fixed digit
# - see ops/tk_focus_item.c (ported from the pal-chain family's own
# proven, project-agnostic version of this exact primitive).
cp "$SESS/pieces/display/current_frame.txt" "$PROOF/02b_before_cancel_frame.txt"
"$HARNESS_DIR/ops/+x/tk_focus_item.+x" "$SESS" "$PROOF/02b_before_cancel_frame.txt" "CANCEL" >/dev/null \
    || fail "CANCEL button not found in the real frame (see 02b_before_cancel_frame.txt)"
inject_key "$SESS" 13
sleep 0.3
jump_to "$SESS" 1                 # NEW FILE
inject_key "$SESS" 13
sleep 2
if ! grep -qF "$MARKER" "$SESS/pieces/system/editor_buffer.txt" 2>/dev/null; then
    pass "real NEW cleared the marker out of editor_buffer.txt"
else
    fail "NEW did not clear the buffer — marker still present"
fi

# --- Navigate to LOAD, real path typing, real LOAD ---
jump_to "$SESS" 4                 # LOAD...
inject_key "$SESS" 13
sleep 0.3
[ "$(current_layout_of "$SESS")" = "pieces/chtpm/layouts/file_browser_load.chtpm" ] \
    && pass "navigated to file_browser_load.chtpm" || fail "did not land on file_browser_load.chtpm"

# Same real fix as the SAVE flow above: item 1 is SEARCH, item 2 is
# the real FILE field, and Enter-while-engaged confirms directly.
jump_to "$SESS" 2                 # FILE field (real item 2, not 1)
inject_key "$SESS" 13
wait_for_typing "$SESS" 30 || { fail "INTERACT never engaged on FILE field (load)"; exit 1; }
"$HARNESS_DIR/ops/+x/tk_type_text.+x" "$SESS" "$SAVE_PATH"
sleep 0.3
inject_key "$SESS" 13             # real Enter-on-FILE-field confirm, not Esc+jump
sleep 2
cp "$SESS/pieces/display/current_frame.txt" "$PROOF/03_after_load_frame.txt"
cp "$SESS/pieces/system/editor_buffer.txt" "$PROOF/04_buffer_after_load.txt" 2>/dev/null

if grep -qF "$MARKER" "$SESS/pieces/system/editor_buffer.txt" 2>/dev/null; then
    pass "real LOAD restored the marker from disk into editor_buffer.txt — full save/load loop proven"
else
    fail "marker not present after LOAD — buffer was not really restored from disk"
fi

rm -f "$REAL_SAVE_TARGET"

echo ""
echo "Proof: $PROOF"
if [ "$FAIL" = "0" ]; then echo "=== OVERALL: PASS ==="
else echo "=== OVERALL: FAIL ==="; fi
exit "$FAIL"
