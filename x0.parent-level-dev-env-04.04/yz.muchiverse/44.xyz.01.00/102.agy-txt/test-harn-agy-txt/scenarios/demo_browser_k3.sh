#!/bin/bash
# demo_browser_k3.sh - real, K3/§39-compliant black-box test of
# agy-txt's own new file browser (ported from &.widgits/file-menu's
# now-fixed handle_file_browser(), see PLAN.md §4 Phase T4/T6 and
# !.xyzos-standards+1.txt §39's own mandatory checklist). Covers every
# item that checklist requires, real key injection throughout, not
# op-level shortcuts:
#   1) real per-character typing into the FILE field (not PASTE alone)
#   2) real Enter-submits AND real Esc-only-deactivates, both
#      directions, including Enter-on-an-empty-field-activates-it
#   3) real click-to-load a LISTED entry (arrow-nav + one real Enter)
#   4) real on-disk file-existence check after SAVE_AS
#   5) real jail-escape attempt, asserting both a real rejection and
#      that nothing landed outside xyzfs
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HARNESS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/common.sh"

FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

cleanup() { stop_app; }
trap cleanup EXIT INT TERM

PROOF="$HARNESS_DIR/proof/browser-k3-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF"

echo "=== demo_browser_k3: agy-txt's new file browser, real K3-parity coverage ==="

LOGIN_FILE="$HOUSE/0.user-pal👤️/00.login-signup/current_login.txt"
XYZFS="$(grep '^current_xyzfs=' "$LOGIN_FILE" | head -1 | cut -d= -f2-)"
XYZFS_DOCS="$HOUSE/$XYZFS/home/documents"
mkdir -p "$XYZFS_DOCS"

jump_to() { # jump_to <session> <n> - digit-jump on a CHTPM-buttoned layout (editor.chtpm/file_menu.chtpm only)
    local session="$1" n="$2"
    inject_key "$session" "$((48 + n))"
}

# wait_for_field_active <session> <timeout_tenths> - poll THIS
# project's own field_active state (agy_browser_state.txt), NOT
# chtpm's native active_gui_is_typing.txt - the new buttonless file
# browser layouts have no onClick="INTERACT" element at all, so that
# flag can never become 1 here; field_active is the real, own-built
# equivalent (ported from fm_menu_input.c, see PITFALL 63/64).
wait_for_field_active() {
    local session="$1" timeout="${2:-30}" waited=0
    local st="$session/pieces/system/agy_browser_state.txt"
    while [ "$(grep '^field_active=' "$st" 2>/dev/null | cut -d= -f2)" != "1" ] && [ "$waited" -lt "$timeout" ]; do
        sleep 0.1; waited=$((waited + 1))
    done
    [ "$(grep '^field_active=' "$st" 2>/dev/null | cut -d= -f2)" = "1" ]
}

browser_state_of() { cat "$1/pieces/system/agy_browser_state.txt" 2>/dev/null; }

# wait_for_kv <session> <key> <expected> <timeout_tenths> - poll for a
# real, exact expected value in agy_browser_state.txt, rather than a
# fixed sleep after inject_string - the FIRST version of this scenario
# checked state via a blind sleep right after typing and caught the
# LAST few characters still mid-flight (confirmed live: "k3-save-
# 21456.txt" landed as "k3-save-21456", missing ".txt" - not a
# functional bug, a settle-time bug in the test itself, same class
# already documented in !.xyzos-standards+1.txt §39's own reliability
# note).
wait_for_kv() {
    local session="$1" key="$2" expected="$3" timeout="${4:-40}" waited=0
    local st="$session/pieces/system/agy_browser_state.txt"
    while [ "$(grep "^${key}=" "$st" 2>/dev/null | cut -d= -f2-)" != "$expected" ] && [ "$waited" -lt "$timeout" ]; do
        sleep 0.1; waited=$((waited + 1))
    done
    [ "$(grep "^${key}=" "$st" 2>/dev/null | cut -d= -f2-)" = "$expected" ]
}

stop_app
start_app
SESS="$(find_session)" || { fail "session launch never appeared"; exit 1; }
echo "SESSION=$SESS" | tee "$PROOF/00_session.txt"

# --- Navigate: editor.chtpm -> FILE MENU -> SAVE AS -> real file_browser_save.chtpm ---
jump_to "$SESS" 2                 # FILE MENU
inject_key "$SESS" 13
sleep 0.3
jump_to "$SESS" 3                 # SAVE AS...
inject_key "$SESS" 13
sleep 0.3
[ "$(current_layout_of "$SESS")" = "pieces/chtpm/layouts/file_browser_save.chtpm" ] \
    && pass "real href navigated to file_browser_save.chtpm" || { fail "did not land on file_browser_save.chtpm"; exit 1; }

# --- 1) real per-character typing into the FILE field (not PASTE) ---
# cursor_pos 0=SEARCH,1=FILE - one real ARROW_DOWN to reach FILE.
echo "1003" >> "$SESS/pieces/apps/player_app/interact_relay.txt"
sleep 0.3
SAVE_NAME="k3-save-$$.txt"
inject_string "$SESS" "$SAVE_NAME"
wait_for_kv "$SESS" "path_buffer" "$SAVE_NAME" 40
cp "$SESS/pieces/system/agy_browser_state.txt" "$PROOF/01_state_after_typing.txt"
if grep -q "^path_buffer=$SAVE_NAME$" "$SESS/pieces/system/agy_browser_state.txt" 2>/dev/null; then
    pass "real per-character typing (not PASTE) landed in path_buffer"
else
    fail "real per-character typing did not land"
fi
if grep -q "^field_active=1$" "$SESS/pieces/system/agy_browser_state.txt" 2>/dev/null; then
    pass "field_active set to 1 by real typing"
else
    fail "field_active was not set by real typing"
fi
grep "FILE:" "$SESS/pieces/apps/player_app/view.txt" 2>/dev/null | tee "$PROOF/01_view_file_line.txt"

# --- 2a) real Esc scoping: first Esc deactivates only, second exits reset ---
echo "27" >> "$SESS/pieces/apps/player_app/interact_relay.txt"
sleep 0.3
cp "$SESS/pieces/system/agy_browser_state.txt" "$PROOF/02_state_after_esc1.txt"
if grep -q "^field_active=0$" "$SESS/pieces/system/agy_browser_state.txt" 2>/dev/null && \
   grep -q "^path_buffer=$SAVE_NAME$" "$SESS/pieces/system/agy_browser_state.txt" 2>/dev/null; then
    pass "first real Esc deactivated typing only, path_buffer preserved"
else
    fail "first real Esc did not behave as deactivate-only"
fi

# --- 2b) real Enter-submits: path_buffer already correctly has
# "$SAVE_NAME" preserved from step 2a's own Esc (typing APPENDS, not
# replaces - re-typing here would double it, confirmed live: the first
# version of this test did exactly that and produced a real disk file
# named "...txtk3-save-....txt"). Real Enter directly on the FILE
# field, submitting the content that's already there. ---
echo "13" >> "$SESS/pieces/apps/player_app/interact_relay.txt"
sleep 1
cp "$SESS/pieces/system/widget_cmds/status.txt" "$PROOF/03_status_after_save.txt" 2>/dev/null
SAVE_TARGET="$XYZFS_DOCS/$SAVE_NAME"
if [ -f "$SAVE_TARGET" ]; then
    pass "real Enter on FILE field submitted; real file exists on disk at $SAVE_TARGET - not just a status message"
else
    fail "no real file landed on disk at $SAVE_TARGET after real Enter-submit"
fi

# --- 2c) Enter-on-empty-field-activates: fresh SAVE_AS entry, empty
# FILE field, Enter FIRST (must activate, not attempt an empty submit).
# Must actually LEAVE and RE-ENTER file_browser_save.chtpm for the
# "fresh entry" reset to trigger (last_layout only differs on a real
# layout change) - real CANCEL button back to file_menu.chtpm, then
# real href back into SAVE AS again. ---
echo "27" >> "$SESS/pieces/apps/player_app/interact_relay.txt"   # Esc (deactivate, if active)
sleep 0.3
echo "27" >> "$SESS/pieces/apps/player_app/interact_relay.txt"   # Esc again (defensive, in case still active)
sleep 0.3
inject_key "$SESS" 13   # real CANCEL button - chtpm's own native button-nav, keyboard/history.txt
sleep 0.3
CUR_LAYOUT_PRE2C="$(current_layout_of "$SESS")"
if [ "$CUR_LAYOUT_PRE2C" != "pieces/chtpm/layouts/file_menu.chtpm" ]; then
    fail "real CANCEL button (step 2c setup) did not return to file_menu.chtpm (got: $CUR_LAYOUT_PRE2C)"
    exit 1
fi
jump_to "$SESS" 3        # SAVE AS... (fresh entry - real reset)
inject_key "$SESS" 13
sleep 0.3
echo "1003" >> "$SESS/pieces/apps/player_app/interact_relay.txt"  # arrow to FILE field
sleep 0.3
echo "13" >> "$SESS/pieces/apps/player_app/interact_relay.txt"    # Enter FIRST, empty field
sleep 0.3
cp "$SESS/pieces/system/agy_browser_state.txt" "$PROOF/04_state_after_enter_first.txt"
if grep -q "^field_active=1$" "$SESS/pieces/system/agy_browser_state.txt" 2>/dev/null; then
    pass "real Enter on an EMPTY, inactive FILE field activated it - did NOT attempt an empty submit"
else
    fail "real Enter-first-to-activate did not engage field_active"
fi

# --- 3) real click-to-load a LISTED entry (LOAD mode) ---
# Navigate: editor.chtpm -> FILE MENU -> LOAD... (need to get back to
# editor.chtpm first via the real CANCEL button, then file_menu).
echo "27" >> "$SESS/pieces/apps/player_app/interact_relay.txt"
sleep 0.3
# real CANCEL button - find its own layout-relative key. This layout
# has exactly one real <button href>; chtpm's own button-nav reaches
# it via keyboard/history.txt regardless of how many virtual browser
# rows exist (a REAL button is independent of the buttonless listing).
inject_key "$SESS" 13
sleep 0.3
CUR_LAYOUT="$(current_layout_of "$SESS")"
echo "layout after CANCEL attempt: $CUR_LAYOUT" | tee -a "$PROOF/05_cancel_nav.txt"
if [ "$CUR_LAYOUT" != "pieces/chtpm/layouts/file_menu.chtpm" ]; then
    fail "real CANCEL button did not return to file_menu.chtpm (see 05_cancel_nav.txt)"
else
    pass "real CANCEL button returned to file_menu.chtpm"
fi

jump_to "$SESS" 4   # LOAD...
inject_key "$SESS" 13
sleep 0.3
[ "$(current_layout_of "$SESS")" = "pieces/chtpm/layouts/file_browser_load.chtpm" ] \
    && pass "real href navigated to file_browser_load.chtpm" || { fail "did not land on file_browser_load.chtpm"; exit 1; }

# Filter to a known-unique fixture via real SEARCH typing, so the
# listed entry's index is deterministic regardless of the user's own
# real files sitting in the same directory (§39's own checklist item).
CLICK_MARKER="CLICK-LOAD-K3-$$-$(date +%s)"
CLICK_FIXTURE_NAME="k3-click-fixture-$$.txt"
printf '%s\n' "$CLICK_MARKER" > "$XYZFS_DOCS/$CLICK_FIXTURE_NAME"
inject_string "$SESS" "$CLICK_FIXTURE_NAME"   # cursor_pos=0 (SEARCH) fresh entry
wait_for_kv "$SESS" "search_text" "$CLICK_FIXTURE_NAME" 40
cp "$SESS/pieces/system/agy_browser_state.txt" "$PROOF/06_state_after_search.txt"

# cursor_pos: 0=SEARCH,1=FILE,2=BACK,3=filtered entry. Three real
# ARROW_DOWN presses.
for i in 1 2 3; do echo "1003" >> "$SESS/pieces/apps/player_app/interact_relay.txt"; sleep 0.2; done
cp "$SESS/pieces/system/agy_browser_state.txt" "$PROOF/07_state_on_entry.txt"
if grep -q "^cursor_pos=3$" "$SESS/pieces/system/agy_browser_state.txt" 2>/dev/null; then
    pass "real arrow navigation reached the filtered listing's own entry (cursor_pos=3)"
else
    fail "arrow navigation did not reach the expected entry index"
fi

echo "13" >> "$SESS/pieces/apps/player_app/interact_relay.txt"
sleep 1
cp "$SESS/pieces/system/editor_buffer.txt" "$PROOF/08_editor_buffer_after_click_load.txt" 2>/dev/null
if grep -qF "$CLICK_MARKER" "$SESS/pieces/system/editor_buffer.txt" 2>/dev/null; then
    pass "ONE real Enter on a listed file entry loaded it directly into the editor buffer - real click-to-load"
else
    fail "editor_buffer.txt does not contain the marker after selecting the listed entry"
fi
rm -f "$XYZFS_DOCS/$CLICK_FIXTURE_NAME"

# --- 4) real jail-escape attempt ---
echo "27" >> "$SESS/pieces/apps/player_app/interact_relay.txt"
sleep 0.3
inject_key "$SESS" 13   # real CANCEL back to file_menu
sleep 0.3
jump_to "$SESS" 3        # SAVE AS...
inject_key "$SESS" 13
sleep 0.3
echo "1003" >> "$SESS/pieces/apps/player_app/interact_relay.txt"
sleep 0.3
ESCAPE_MARKER="ESCAPE-K3-$$"
DOTDOTS="../../../../../../../../../../../../../../../../../../../../../../../../../../../../../.."
inject_string "$SESS" "${DOTDOTS}/tmp/${ESCAPE_MARKER}.txt"
wait_for_kv "$SESS" "path_buffer" "${DOTDOTS}/tmp/${ESCAPE_MARKER}.txt" 90
echo "13" >> "$SESS/pieces/apps/player_app/interact_relay.txt"
sleep 1
cp "$SESS/pieces/system/widget_cmds/status.txt" "$PROOF/09_status_after_escape.txt" 2>/dev/null
if [ -f "$SESS/pieces/system/widget_cmds/status.txt" ] && grep -q '^result=error$' "$SESS/pieces/system/widget_cmds/status.txt"; then
    pass "real jail-escape attempt was REJECTED with a real error status"
else
    fail "real jail-escape attempt did NOT produce a real error status"
fi
if [ ! -f "/tmp/${ESCAPE_MARKER}.txt" ]; then
    pass "no file actually landed outside xyzfs at the real escape target"
else
    fail "REAL SECURITY FAILURE: a file landed outside xyzfs at /tmp/${ESCAPE_MARKER}.txt"
    rm -f "/tmp/${ESCAPE_MARKER}.txt"
fi

rm -f "$SAVE_TARGET"

echo ""
echo "Proof: $PROOF"
if [ "$FAIL" = "0" ]; then echo "=== OVERALL: PASS ==="
else echo "=== OVERALL: FAIL ==="; fi
exit "$FAIL"
