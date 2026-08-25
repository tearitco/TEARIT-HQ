# common.sh - shared helpers for test-harn-agy-txt scenarios.
# Adapted from @.apps/text-editor-xyz/test-harn-ed-app/scenarios/
# common.sh (real, proven §36.6-level-2 shape - this project has no
# cross-project session discovery to do, single project, single
# session, so this is a trimmed-down version, not a from-scratch one).

AGY_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
HOUSE="$(cd "$AGY_DIR/.." && pwd)"

start_app() {
    cd "$AGY_DIR"
    rm -rf pieces/sessions
    NO_GL=1 setsid bash button.sh run > "$HARNESS_DIR/proof/app_stdout.log" 2>&1 < /dev/null &
    disown
}

stop_app() {
    bash "$HOUSE/EMERGENCY_KILL.sh" >/dev/null 2>&1 || true
}

# find_session - polls for the freshest session dir with a real
# current_frame.txt (proves chtpm_parser_pal has actually composed at
# least once, same readiness check every real harness in this house
# family uses - not a fixed sleep guess).
find_session() {
    local waited=0
    while [ "$waited" -lt 100 ]; do
        local candidate
        candidate="$(ls -dt "$AGY_DIR"/pieces/sessions/*/ 2>/dev/null | head -1)"
        if [ -n "$candidate" ] && [ -f "${candidate}pieces/display/current_frame.txt" ]; then
            echo "${candidate%/}"
            return 0
        fi
        sleep 0.1
        waited=$((waited + 1))
    done
    return 1
}

# inject_key <session> <keycode> - real buttoned-layout channel
# (pieces/keyboard/history.txt, "[TS] KEY_PRESSED: N" format) - matches
# this project's own real editor.chtpm/file_menu.chtpm/file_browser_*
# layouts exactly (all buttoned, none of the buttonless "dumb ASCII
# menu" shape file-menu widget uses - see !.xyzos-standards+1.txt §36.6
# for why the channel choice matters and must match the real layout).
inject_key() {
    local session="$1" code="$2"
    echo "[TS] KEY_PRESSED: $code" >> "$session/pieces/keyboard/history.txt"
    sleep 0.2
}

inject_repeat() {
    local session="$1" code="$2" n="$3" i
    for (( i=0; i<n; i++ )); do inject_key "$session" "$code"; done
}

# inject_string <session> <text> - real per-CHARACTER injection via
# interact_relay.txt. KNOWN UNRELIABLE under automation (2026-07-30):
# test-harn-ed-app's own demo_save.sh hit this EXACT same symptom first
# (char-by-char through the relay hop truncated/dropped a marker string
# even with the 50ms throttle) and fixed it by switching to a real
# PASTE-mode op call instead - see ag_paste() below. Kept here only for
# completeness/single-char use; do NOT use for multi-char marker/path
# strings in scenarios, use ag_paste.
inject_string() {
    local session="$1" text="$2"
    local relay="$session/pieces/apps/player_app/interact_relay.txt"
    local i c code
    for (( i=0; i<${#text}; i++ )); do
        c="${text:$i:1}"
        code=$(printf '%d' "'$c")
        echo "$code" >> "$relay"
        sleep 0.05
    done
}

# ag_paste <session> <string> - real PASTE-mode call into the real
# agy_edit_key.+x op (inherited verbatim from editor_menu_input.c's own
# PASTE addition - see agy_edit_key.c's own main() comment). Inserts
# the whole string at once at the cursor (editor buffer or path field,
# whichever is active - the op itself branches on current_layout.txt),
# bypassing interact_relay.txt's per-character relay hop entirely, same
# fix shape as test-harn-ed-app's ed_paste(). Only valid while INTERACT
# is actually engaged (same real gate insert_char()/insert_path_string()
# already enforce) - a no-op otherwise, not a bypass of that gate.
ag_paste() {
    local session="$1" text="$2"
    PRISC_PROJECT_ROOT="$session" "$AGY_DIR/ops/+x/agy_edit_key.+x" PASTE "$text" >/dev/null 2>&1
}

wait_for_typing() {
    local session="$1" timeout="${2:-30}" waited=0
    local flag="$session/pieces/display/active_gui_is_typing.txt"
    while [ "$(cat "$flag" 2>/dev/null)" != "1" ] && [ "$waited" -lt "$timeout" ]; do
        sleep 0.1; waited=$((waited + 1))
    done
    [ "$(cat "$flag" 2>/dev/null)" = "1" ]
}

current_layout_of() {
    cat "$1/pieces/display/current_layout.txt" 2>/dev/null
}
