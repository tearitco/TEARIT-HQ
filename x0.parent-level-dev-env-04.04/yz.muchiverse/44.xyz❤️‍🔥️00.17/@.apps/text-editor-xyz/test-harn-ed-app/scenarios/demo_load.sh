#!/bin/bash
# demo_load.sh - proves LOAD works end-to-end through REAL entry
# points and REAL key injection (§36.6 level 2), independent of
# demo_save.sh (modular — either scenario runs standalone).
#
# Flow: launch the real combined app -> navigate file-menu's REAL menu
# via injected keys (LOAD -> type absolute fixture path -> confirm) ->
# assert editor's buffer AND its rendered frame both show the loaded
# content (state-file assertion alone is exactly what the old
# level-1-only harness could do; the frame assertion is the part it
# could never catch — see !.xyzos-standards+1.txt §36.6).
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

FAIL=0
pass() { report PASS "$1"; }
fail() { report FAIL "$1"; FAIL=1; }

cleanup() { stop_app; }
trap cleanup EXIT INT TERM

PROOF="$HARNESS/proof/load-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF"

echo "=== demo_load: LOAD via real key injection ==="

# Unique fixture content so a false-positive (stale buffer, wrong
# file) is impossible to mistake for a real pass.
#
# Fixture placed directly under the REAL xyzfs documents/ location
# (2026-07-30, save-bug.txt's own fix, direct instruction: LOAD is now
# jailed to the user's own xyzfs home the same way SAVE_AS is - a
# leading "/" resolves relative to THAT root, never the real host
# filesystem, so an absolute host fixture path like the old
# $HARNESS/fixtures/... would no longer be reachable at all). A BARE
# filename is what a real user LOAD flow actually types/pastes, so
# that's what this harness pastes too - matching demo_save.sh's own
# identical fix.
MARKER="HARNESS-LOAD-MARKER-$$-$(date +%s)"
LOGIN_FILE="$HOUSE/0.user-pal👤️/00.login-signup/current_login.txt"
XYZFS="$(grep '^current_xyzfs=' "$LOGIN_FILE" | head -1 | cut -d= -f2-)"
FIXTURE_NAME="load_target_$$.txt"
FIXTURE="$HOUSE/$XYZFS/home/documents/$FIXTURE_NAME"
mkdir -p "$HOUSE/$XYZFS/home/documents"
printf '%s\nsecond line of the fixture\n' "$MARKER" > "$FIXTURE"
cp "$FIXTURE" "$PROOF/00_fixture.txt"

sh "$HOUSE/EMERGENCY_KILL.sh" >/dev/null 2>&1 || true
start_app

EDITOR_SESSION="$(find_editor_session)" || { fail "editor session never appeared"; exit 1; }
FM_SESSION="$(find_fm_session)" || { fail "file-menu session never appeared"; exit 1; }
echo "EDITOR_SESSION=$EDITOR_SESSION" | tee -a "$PROOF/01_sessions.txt"
echo "FM_SESSION=$FM_SESSION" | tee -a "$PROOF/01_sessions.txt"

# 2026-07-30 rebuild (PITFALL 65 fix): file-menu's own layout now has
# REAL chtpm-native <button>/<cli_io> elements - real key injection
# goes through pieces/keyboard/history.txt (fm_inject_key/fm_inject_
# string, common.sh), the same real channel chtpm_parser_pal.c's own
# nav/focus/digit-jump/Enter-activate logic reads directly, matching
# 102.editor's own layout exactly. interact_relay.txt is now purely
# the OUTPUT channel a real button activation (or a real cli_io
# "Send" - see fm_menu_input.c's own active_gui_index() header
# comment) relays a synthetic value into - not something a test
# writes to directly anymore.
FM_HIST="$FM_SESSION/pieces/keyboard/history.txt"
wait_for_path "$FM_HIST" 50 || { fail "file-menu keyboard/history.txt never appeared"; exit 1; }

# Real digit-jump to LOAD FILE... (real chtpm digit-jump, JUMP-ONLY -
# a separate real Enter activates it), a real href into
# file_menu_browser_load.chtpm.
fm_inject_key "$FM_SESSION" 52
sleep 0.3
fm_inject_key "$FM_SESSION" 13
sleep 0.8

# Real nav to the FILE cli_io: SEARCH is the first navigable element
# in the fresh file_menu_browser_load.chtpm structure (real, clean
# focus reset on every href transition - chtpm_parser_pal.c's own
# clear_saved_active_index() fix, 2026-07-30), one DOWN reaches FILE,
# Enter engages it for real per-character typing.
fm_inject_key "$FM_SESSION" 1003
sleep 0.2
fm_inject_key "$FM_SESSION" 13
sleep 0.3
# Real per-character typing of the fixture's BARE FILENAME (not an
# absolute path - a bare name resolves under the default browse_dir,
# the real xyzfs documents/ folder). No PASTE mode exists for cli_io
# fields anymore (fm_menu_input.c no longer owns their content at all -
# chtpm_parser_pal does, natively); real per-character injection is
# the only mechanism now, matching a real user's own typing exactly.
fm_inject_string "$FM_SESSION" "$FIXTURE_NAME"
sleep 0.3
# Real Enter directly on the FILE field submits (real chtpm cli_io
# "Send" relay, 2026-07-30 - matches TPMOS's own Enter-on-file_path_
# input=SET_LOAD_ACTION convention) - no separate CONFIRM button
# activation needed.
fm_inject_key "$FM_SESSION" 13
sleep 0.3

echo "--- injected key sequence (file-menu, via keyboard/history.txt) ---" | tee -a "$PROOF/02_injected_keys.txt"
tail -n 60 "$FM_HIST" >> "$PROOF/02_injected_keys.txt"
echo "--- relayed KEY:n/Send activations (file-menu, via interact_relay.txt) ---" | tee -a "$PROOF/02_injected_keys.txt"
tail -n 20 "$FM_SESSION/pieces/apps/player_app/interact_relay.txt" >> "$PROOF/02_injected_keys.txt"

# editor_widget_cmds's own background drain loop (started by this
# app's button.sh, step 6/97 of @.apps/text-editor-xyz/button.sh)
# polls the inbox every 0.2s — give it real margin.
sleep 2

BUFFER="$EDITOR_SESSION/pieces/system/editor_buffer.txt"
FRAME="$EDITOR_SESSION/pieces/display/current_frame.txt"
STATUS="$EDITOR_SESSION/pieces/system/widget_cmds/status.txt"
cp "$BUFFER" "$PROOF/03_editor_buffer.txt" 2>/dev/null
cp "$FRAME" "$PROOF/03_current_frame.txt" 2>/dev/null
cp "$STATUS" "$PROOF/03_status.txt" 2>/dev/null

if [ -f "$BUFFER" ] && diff -q "$FIXTURE" "$BUFFER" >/dev/null 2>&1; then
    pass "editor_buffer.txt byte-for-byte matches fixture"
else
    fail "editor_buffer.txt does NOT match fixture (state-file check)"
fi

if [ -f "$FRAME" ] && grep -qF "$MARKER" "$FRAME"; then
    pass "current_frame.txt (RENDERED frame) contains the marker — level-2 proof, not just state"
else
    fail "current_frame.txt does NOT show the marker — rendered frame check FAILED"
fi

if [ -f "$STATUS" ] && grep -q '^last_cmd=LOAD$' "$STATUS" && grep -q '^result=ok$' "$STATUS"; then
    pass "widget_cmds/status.txt shows last_cmd=LOAD result=ok"
else
    fail "widget_cmds/status.txt does not confirm LOAD ok"
fi

echo ""
echo "Proof: $PROOF"
if [ "$FAIL" = "0" ]; then
    echo "=== demo_load: ALL PASS ==="
else
    echo "=== demo_load: FAILURES ABOVE — see $PROOF ==="
fi
exit "$FAIL"
