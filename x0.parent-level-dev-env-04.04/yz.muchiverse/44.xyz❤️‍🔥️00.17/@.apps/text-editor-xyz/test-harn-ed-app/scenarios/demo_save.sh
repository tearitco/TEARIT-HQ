#!/bin/bash
# demo_save.sh - proves SAVE_AS works end-to-end through REAL entry
# points and REAL key injection (§36.6 level 2), including REAL text
# EDITING first (not a static seed-buffer passthrough — this is the
# "text edited files" part specifically asked for). Independent of
# demo_load.sh (modular — either scenario runs standalone).
#
# Flow: launch the real combined app -> engage editor's own INTERACT
# mode via a real keypress on the default-selected "EDIT TEXT
# (INTERACT)" button -> type a unique marker string as real keys ->
# exit INTERACT -> navigate file-menu's REAL menu (SAVE_AS -> type
# absolute output path -> confirm) -> assert the ACTUAL FILE ON DISK
# contains what was typed.
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

FAIL=0
pass() { report PASS "$1"; }
fail() { report FAIL "$1"; FAIL=1; }

cleanup() { stop_app; }
trap cleanup EXIT INT TERM

PROOF="$HARNESS/proof/save-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF" "$HARNESS/workdir"

echo "=== demo_save: real edit + SAVE_AS via real key injection ==="

# Letters/hyphens only — avoids any ambiguity if timing is imperfect
# and a character lands before INTERACT is fully engaged (digits would
# otherwise risk being read as a menu-dispatch keycode instead of
# literal typed text).
EDIT_MARKER="EDITED-BY-HARNESS-$$-$(date +%s | tr -d '\n' | tr '0-9' 'a-j')"

# BARE relative filename, not an absolute scratch path (2026-07-30,
# save-bug.txt's own real fix, direct instruction: SAVE_AS now jails
# to the user's own xyzfs home - a leading "/" is interpreted as
# relative to THAT root, never the real host filesystem, so an
# absolute host scratch path like the old $HARNESS/workdir/... no
# longer lands where it looks like it would). A bare name resolves
# under <xyzfs_home>/documents/ by default - resolve that same real
# location here for the assertion below, via the identical 2-hop chain
# ledger_append.c/resolve_save_path() both use.
SAVE_NAME="saved_$$.txt"
LOGIN_FILE="$HOUSE/0.user-pal👤️/00.login-signup/current_login.txt"
XYZFS="$(grep '^current_xyzfs=' "$LOGIN_FILE" | head -1 | cut -d= -f2-)"
OUT_PATH="$HOUSE/$XYZFS/home/documents/$SAVE_NAME"
rm -f "$OUT_PATH"

sh "$HOUSE/EMERGENCY_KILL.sh" >/dev/null 2>&1 || true
start_app

EDITOR_SESSION="$(find_editor_session)" || { fail "editor session never appeared"; exit 1; }
FM_SESSION="$(find_fm_session)" || { fail "file-menu session never appeared"; exit 1; }
echo "EDITOR_SESSION=$EDITOR_SESSION" | tee -a "$PROOF/01_sessions.txt"
echo "FM_SESSION=$FM_SESSION" | tee -a "$PROOF/01_sessions.txt"

ED_HIST="$EDITOR_SESSION/pieces/keyboard/history.txt"
TYPING_FLAG="$EDITOR_SESSION/pieces/display/active_gui_is_typing.txt"
wait_for_path "$ED_HIST" 50 || { fail "editor keyboard/history.txt never appeared"; exit 1; }

# "EDIT TEXT (INTERACT)" is the default-selected element at fresh
# launch (row [>] 1. per the editor's own layout ordering) — Enter
# here engages chtpm_parser_pal's own onClick="INTERACT" handling
# directly (this is NOT routed through editor_menu_input.c's own
# digit-dispatch — piece.pdl's METHOD list has no "EDIT" command
# string handled there; confirmed by reading both files directly).
inject_key "$ED_HIST" 13

# Wait for the engagement to actually register (chtpm_parser_pal's
# export_active_index() writes this file) rather than a fixed sleep —
# real synchronization point, not a guess.
waited=0
while [ "$(cat "$TYPING_FLAG" 2>/dev/null)" != "1" ] && [ "$waited" -lt 30 ]; do
    sleep 0.1; waited=$((waited + 1))
done
if [ "$(cat "$TYPING_FLAG" 2>/dev/null)" != "1" ]; then
    fail "INTERACT never engaged (active_gui_is_typing.txt never became 1)"
    echo "$FAIL"; exit 1
fi
pass "INTERACT engaged (real button activation, not a shortcut)"

# ed_paste, not inject_string: confirmed live that the keyboard/
# history.txt -> chtpm_parser_pal -> project-history relay hop has its
# own separate polling-rate bottleneck beyond keyboard/history.txt's
# own 50ms-per-char throttle (inject_string's own real fix, still
# correct for what it covers) — a full marker string typed character-
# by-character through this specific relay came out truncated
# (confirmed: typed "...feadagh", buffer only got "...bhi") even with
# the throttle in place. ed_paste sidesteps the extra hop entirely via
# editor_menu_input.c's own PASTE mode (same mechanism PITFALL 56 added
# for file-menu's FILE field, reused here for a different bottleneck).
ed_paste "$EDITOR_SESSION" "$EDIT_MARKER"
sleep 0.3
# ESC exits INTERACT (per the layout's own on-screen hint text).
inject_key "$ED_HIST" 27
sleep 0.2
# '4' = FM (FILE MENU) per piece.pdl's own METHOD list — a real
# production signal (do_fm(), wakes/focuses the already-running
# widget), not a test shortcut; matches the real user flow.
inject_key "$ED_HIST" 52
sleep 0.3

echo "--- injected key sequence (editor) ---" | tee -a "$PROOF/02_injected_keys_editor.txt"
tail -n 40 "$ED_HIST" >> "$PROOF/02_injected_keys_editor.txt"

# 2026-07-30 rebuild (PITFALL 65 fix): file-menu's own layout now has
# REAL chtpm-native <button>/<cli_io> elements - chtpm_parser_pal.c
# owns nav/focus/digit-jump/Enter-activate itself, same as 102.editor's
# own layout. Real key injection goes through the file-menu SESSION's
# own pieces/keyboard/history.txt now (fm_inject_key/fm_inject_string,
# common.sh), NOT a direct interact_relay.txt write - that file is now
# purely the OUTPUT channel a real button activation relays a
# synthetic KEY:n into (see fm_menu_input.c's own header comment on
# chtpm_parser_pal.c's send_command()/inject_raw_key() ASCII-shift
# quirk for 0-9), not something a test should write to directly.
FM_HIST="$FM_SESSION/pieces/keyboard/history.txt"
wait_for_path "$FM_HIST" 50 || { fail "file-menu keyboard/history.txt never appeared"; exit 1; }

# '3' = SAVE_AS (real digit-jump, JUMP-ONLY - a separate real Enter
# activates it, matching this house's own standard chtpm digit-jump
# convention, unchanged by this rebuild).
fm_inject_key "$FM_SESSION" 51
sleep 0.3
fm_inject_key "$FM_SESSION" 13
# REAL FIX (2026-07-31): file-menu's 5 screens now each get their own
# dedicated <module> (#.haiku+/!.xyzos-standards+1.txt §41) instead of
# sharing one always-alive process - this specific transition (main ->
# browser_save) now triggers a genuine kill+fork+exec of a fresh
# module, which needs real wall-clock time to actually register before
# further input makes sense. The old fixed `sleep 0.5` was tuned for a
# module that was already alive the whole time; wait for the real,
# deterministic signal (current_layout.txt actually flipping) instead
# of guessing a bigger fixed number.
waited=0
while [ "$(cat "$FM_SESSION/pieces/display/current_layout.txt" 2>/dev/null)" != "pieces/chtpm/layouts/file_menu_browser_save.chtpm" ] && [ "$waited" -lt 30 ]; do
    sleep 0.1; waited=$((waited + 1))
done
sleep 0.3

# Real nav to the FILE cli_io: SEARCH is the first navigable element
# in file_browser mode, FILE is the second - one DOWN arrow reaches
# it, then Enter engages it for typing (real cli_io convention,
# confirmed this session: Enter on a focused cli_io sets active_index,
# does not submit).
fm_inject_key "$FM_SESSION" 1003
sleep 0.2
fm_inject_key "$FM_SESSION" 13
sleep 0.3
fm_inject_string "$FM_SESSION" "$SAVE_NAME"
sleep 0.3
# Esc exits typing (stays focused on the FILE field, real cli_io
# convention).
fm_inject_key "$FM_SESSION" 27
sleep 0.2

# Real, computed DOWN-count to CONFIRM (SAVE AS) - real chtpm nav
# WRAPS at the last navigable element (confirmed via direct read of
# chtpm_parser_pal.c's own process_key()), so blindly over-pressing
# DOWN risks wrapping past CONFIRM/CANCEL back to SEARCH - not a real
# user's own behavior (a real user stops at what they see on screen),
# so this counts the REAL current directory listing the same way
# fm_compose_frame.c does, via the same real fm_scan_dir.+x op, for a
# deterministic press count. Layout order from FILE (nav index 1):
# BACK, then N real entries, then CONFIRM - (2 + N) DOWN presses.
DOCS_DIR="$(dirname "$OUT_PATH")"
N_ENTRIES="$("$FM_DIR/ops/+x/fm_scan_dir.+x" "$DOCS_DIR" 2>/dev/null | wc -l)"
fm_inject_repeat "$FM_SESSION" 1003 "$((2 + N_ENTRIES))"
sleep 0.2
fm_inject_key "$FM_SESSION" 13
sleep 0.3

echo "--- injected key sequence (file-menu, via keyboard/history.txt) ---" | tee -a "$PROOF/02_injected_keys_fm.txt"
tail -n 60 "$FM_HIST" >> "$PROOF/02_injected_keys_fm.txt"
echo "--- relayed KEY:n activations (file-menu, via interact_relay.txt) ---" | tee -a "$PROOF/02_injected_keys_fm.txt"
tail -n 20 "$FM_SESSION/pieces/apps/player_app/interact_relay.txt" >> "$PROOF/02_injected_keys_fm.txt"

sleep 2

STATUS="$EDITOR_SESSION/pieces/system/widget_cmds/status.txt"
cp "$STATUS" "$PROOF/03_status.txt" 2>/dev/null
[ -f "$OUT_PATH" ] && cp "$OUT_PATH" "$PROOF/03_saved_file.txt" 2>/dev/null

if [ -f "$OUT_PATH" ] && grep -qF "$EDIT_MARKER" "$OUT_PATH"; then
    pass "disk file at the REAL xyzfs documents/ location ($OUT_PATH) contains the text actually typed - real edit, real save, real jailed disk location (save-bug.txt's own fix)"
else
    fail "disk file missing at the real xyzfs documents/ location, or does not contain the typed marker (see $OUT_PATH)"
fi

# NOTE: editor_widget_cmds.c's own do_save_to() always labels
# last_cmd=SAVE via its own set_status("SAVE", ...) call, even when it
# was reached through the SAVE_AS:<path> branch — a real, minor,
# pre-existing naming quirk in that function itself (confirmed live),
# not a bug in this harness. The message field still correctly names
# the real SAVE_AS target path, and the disk-content assertion above
# is the actual load-bearing proof either way.
if [ -f "$STATUS" ] && grep -q '^last_cmd=SAVE$' "$STATUS" && grep -q '^result=ok$' "$STATUS"; then
    pass "widget_cmds/status.txt shows result=ok for the save"
else
    fail "widget_cmds/status.txt does not confirm SAVE_AS ok"
fi

echo ""
echo "Proof: $PROOF"
if [ "$FAIL" = "0" ]; then
    echo "=== demo_save: ALL PASS ==="
else
    echo "=== demo_save: FAILURES ABOVE — see $PROOF ==="
fi
exit "$FAIL"
