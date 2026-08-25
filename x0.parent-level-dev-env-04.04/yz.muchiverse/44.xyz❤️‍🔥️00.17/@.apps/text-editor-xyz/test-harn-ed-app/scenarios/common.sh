#!/bin/bash
# common.sh - shared setup/teardown/injection helpers for
# test-harn-ed-app scenarios (source, don't execute directly).
#
# LEVEL-2 (§36.6) BY CONSTRUCTION.
#
# ARCHITECTURE CHANGE, 2026-07-30 (editor-widget-app-refactor-j30.txt /
# PITFALL 65 fix): &.widgits/file-menu's own layout is NO LONGER the
# "dumb ASCII text menu, PAL-native, buttonless" shape the note below
# used to document - it now has REAL <button>/<cli_io> elements
# (chtpm_parser_pal.c's own is_interactive() gates on exactly those,
# plus <canvas>/<scroller>), matching 102.editor's own shape. This
# means the ORIGINAL §36.6 rule is correct again for file-menu too:
# real key injection goes through pieces/keyboard/history.txt (fm_
# inject_key()/fm_inject_string() below), which chtpm_parser_pal's own
# real nav/focus/digit-jump/Enter-activate logic processes - a real
# button activation (onClick="KEY:n") is what THEN relays a bare
# integer into interact_relay.txt for fm_menu_input.c to read, not
# something a test should write there directly anymore. The OLD
# fm_relay_key()/fm_paste() functions (direct interact_relay.txt
# writes, bypassing chtpm_parser_pal's own nav entirely) matched the
# OLD hand-drawn-marker architecture exactly - they are WRONG for the
# rebuilt layout (would skip real focus/highlighting the parser now
# owns) and have been removed rather than kept as a stale shortcut.
set -u

HARNESS="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_DIR="$(cd "$HARNESS/.." && pwd)"
HOUSE="$(cd "$APP_DIR/../.." && pwd)"
EDITOR_DIR="$(ls -d "$HOUSE"/102.*editor* 2>/dev/null | head -1)"
FM_DIR="$(ls -d "$HOUSE"/\&.widgits/file-menu* 2>/dev/null | head -1)"

# --- key injection (§36.6 mandatory mechanics) ---------------------

# inject_key <history_file> <keycode>
inject_key() {
    local hist="$1" code="$2"
    local ts
    ts="$(date '+%Y-%m-%d %H:%M:%S')"
    echo "[$ts] KEY_PRESSED: $code" >> "$hist"
}

# inject_string <history_file> <string> - one KEY_PRESSED line per
# character, ASCII code via printf/od. Only 7-bit printable input
# expected (paths, plain-ASCII test text) - matches insert_char()'s
# own 32-126 acceptance range in editor_menu_input.c/fm_menu_input.c.
inject_string() {
    local hist="$1" str="$2"
    local i ch code
    for (( i=0; i<${#str}; i++ )); do
        ch="${str:$i:1}"
        code=$(printf '%d' "'$ch")
        inject_key "$hist" "$code"
        # PAL loop processes one key per ~16.7ms iteration (main_loop_
        # chtpm.pal's own `sleep 16667`) — writing faster than that
        # drops characters (confirmed live: no delay -> path_buffer
        # truncated after ~12 chars; 50ms delay -> exact match, zero
        # loss). Real timing constraint, not an arbitrary safety margin.
        sleep 0.05
    done
}

# inject_repeat <history_file> <keycode> <count>
inject_repeat() {
    local hist="$1" code="$2" n="$3" i
    for (( i=0; i<n; i++ )); do inject_key "$hist" "$code"; sleep 0.05; done
}

# --- file-menu's own real key injection (2026-07-30 rebuild - see
# this file's own header comment: real chtpm-native buttons/cli_io
# now, same channel/format as inject_key()/inject_string() above, just
# targeting the file-menu session's own pieces/keyboard/history.txt
# instead of the editor's) -------------------------------------------

# fm_inject_key <fm_session> <keycode>
fm_inject_key() {
    local session="$1" code="$2"
    inject_key "$session/pieces/keyboard/history.txt" "$code"
}

# fm_inject_string <fm_session> <string> - real per-character typing,
# only valid while a real <cli_io> is actively engaged (Enter-focused),
# same real gate chtpm_parser_pal.c's own cli_io input_buffer handling
# enforces. No PASTE-mode equivalent exists anymore for file-menu's own
# cli_io fields (fm_menu_input.c no longer owns their content at all -
# chtpm_parser_pal does, natively) - emoji/multi-byte path segments
# still can't be typed character-by-character through this for the
# same real reason PASTE existed originally; that's a known, real,
# unresolved gap for THIS specific case, not silently worked around.
fm_inject_string() {
    local session="$1" str="$2"
    inject_string "$session/pieces/keyboard/history.txt" "$str"
}

# fm_inject_repeat <fm_session> <keycode> <count>
fm_inject_repeat() {
    local session="$1" code="$2" n="$3"
    inject_repeat "$session/pieces/keyboard/history.txt" "$code" "$n"
}

# ed_paste <editor_session> <string> - inserts at cursor, via the real
# editor_menu_input.+x op. Requires INTERACT already engaged (same
# real gate insert_char() has).
ed_paste() {
    local session="$1" text="$2"
    PRISC_PROJECT_ROOT="$session" "$EDITOR_DIR/ops/+x/editor_menu_input.+x" PASTE "$text" >/dev/null 2>&1
}

# --- polling ---------------------------------------------------------

# wait_for_path <path> <timeout_tenths>
wait_for_path() {
    local path="$1" timeout="${2:-50}" waited=0
    while [ ! -e "$path" ] && [ "$waited" -lt "$timeout" ]; do
        sleep 0.1; waited=$((waited + 1))
    done
    [ -e "$path" ]
}

# wait_for_glob <parent_dir> <pattern> <timeout_tenths> - prints match on stdout
wait_for_glob() {
    local parent="$1" pattern="$2" timeout="${3:-50}" waited=0 hit=""
    while [ "$waited" -lt "$timeout" ]; do
        # -d: list the matched dir ENTRY itself, not its contents
        # (plain `ls -t <dir>` lists what's INSIDE the dir — a real
        # bug this exact helper hit live: printed "debug.txt", the
        # newest file inside the session dir, instead of the session
        # dir's own path).
        hit="$(ls -td "$parent"/$pattern 2>/dev/null | head -1)"
        [ -n "$hit" ] && { echo "$hit"; return 0; }
        sleep 0.1; waited=$((waited + 1))
    done
    return 1
}

# --- session lifecycle ------------------------------------------------

APP_PID=""

start_app() {
    # PITFALL 53: stdin from /dev/null so keyboard_input never touches
    # a real controlling tty at all - sidesteps the SIGTTIN group-stop
    # entirely, rather than relying on "just don't background it" (a
    # harness has to background it to keep injecting after launch).
    bash "$APP_DIR/button.sh" run </dev/null >"$HARNESS/proof/app_stdout.log" 2>&1 &
    APP_PID=$!
}

stop_app() {
    [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null
    sleep 0.3
    sh "$HOUSE/EMERGENCY_KILL.sh" >/dev/null 2>&1 || true
}

# find_editor_session - prints the /tmp/.text-editor-xyz-editor-* dir
find_editor_session() {
    wait_for_glob /tmp ".text-editor-xyz-editor-*" 80
}

# find_fm_session - prints the file-menu widget's own session dir
find_fm_session() {
    wait_for_glob "$FM_DIR/pieces/sessions" "*" 80
}

report() {
    local status="$1" msg="$2"
    echo "$status: $msg"
}
