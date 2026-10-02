#!/bin/bash
# scripts/e2e.sh - end-to-end proof for HORN_CHAT.
#
# Boots the real 3-process stack (renderer + chtpm_parser_pal + the pal
# module the layout forks), drives real keystrokes through the same
# pieces/keyboard/history.txt file keyboard_input writes, and asserts on
# the real frame the renderer would draw. No mocking anywhere: this is
# the whole harness, driven headlessly.
#
# Usage: bash scripts/e2e.sh [clean|keep]
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR" || exit 1

export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
export PRISC_PROJECT_ID="hai-horn"
export HORN_SESSIONS="$SCRIPT_DIR/chats/HORN_SESSIONS"
# HORN_ENTITY_DIR: keys live per-TREE. A git worktree has its own
# &.widgits copy, and key files pasted into the main checkout are NOT
# there - so defaulting to the worktree's copy silently runs the whole
# suite against whatever stale key is committed. An externally supplied
# HORN_ENTITY_DIR always wins.
export HORN_ENTITY_DIR="${HORN_ENTITY_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)/&.widgits/open-hai/state}"

# Load provider keys from the key files, so the suite does not depend on
# the caller having exported them. Every previous run of this script was
# really testing "is GROQ_API_KEY set in this particular shell" until this
# was added - a green run meant nothing about the harness.
# Keys already in the environment win, so CI can inject them directly.
# Look beside the state dir actually in use, not beside the project.
_HOUSE="$(cd "$HORN_ENTITY_DIR" && pwd)"
[ -n "${GROQ_API_KEY:-}" ]        || { [ -f "$_HOUSE/&.widgits/open-hai/state/raw_groq.txt" ]           && export GROQ_API_KEY="$(tr -d ' \t\n\r' < "$_HOUSE/&.widgits/open-hai/state/raw_groq.txt")"; }
[ -n "${HORN_POOLSIDE_KEY:-}" ]   || { [ -f "$_HOUSE/&.widgits/open-hai/state/raw_poolside.txt" ]        && export HORN_POOLSIDE_KEY="$(tr -d ' \t\n\r' < "$_HOUSE/&.widgits/open-hai/state/raw_poolside.txt")"; }
[ -n "${HORN_API_KEY:-}" ]        || { [ -f "$_HOUSE/&.widgits/open-hai/state/openrouter_api_key.txt" ]  && export HORN_API_KEY="$(tr -d ' \t\n\r' < "$_HOUSE/&.widgits/open-hai/state/openrouter_api_key.txt")"; }

KEYS="pieces/keyboard/history.txt"
FRAME="pieces/display/current_frame.txt"
TRANSCRIPT="chats/HORN_SESSIONS/transcript.txt"

pass=0; fail=0
ok()   { echo "  PASS  $1"; pass=$((pass+1)); }
bad()  { echo "  FAIL  $1"; fail=$((fail+1)); }
check(){ if printf '%s' "$2" | grep -qF "$3"; then ok "$1"; else bad "$1 (want '$3' in: $(printf '%s' "$2" | tr '\n' '|' | cut -c1-90))"; fi; }

# Type a string one keystroke at a time, the way keyboard_input does, and
# WAIT for each character to land before sending the next.
#
# chtpm tails pieces/keyboard/history.txt asynchronously, so a fixed sleep
# races it. Appending a burst faster than the parser drains means characters
# arrive while the 200-backspace clear is still being processed, and the
# composer ends up with a mangled remnant - observed live as a completion
# test that actually sent "t".
#
# The wait is on gui_state, which chtpm rewrites per keystroke, so this is
# the parser's own view of the buffer rather than a guess at a delay.
type_str() {
    local s="$1" i want
    for (( i=0; i<${#s}; i++ )); do
        printf 'KEY_PRESSED: %d\n' "'${s:$i:1}" >> "$KEYS"
        want="${s:0:$((i+1))}"
        local t landed=0
        for (( t=0; t<60; t++ )); do
            if grep -qF "horn_prompt=$want" pieces/apps/player_app/manager/gui_state.txt 2>/dev/null; then
                landed=1
                break
            fi
            sleep 0.15
        done
        if [ "$landed" = 0 ]; then
            TYPE_STALLED="${TYPE_STALLED:-}${TYPE_STALLED:+$TYPE_STALLED,}char '$want'"
        fi
    done
    sleep 0.3
}
key() { printf 'KEY_PRESSED: %d\n' "$1" >> "$KEYS"; sleep 0.25; }
esc() { key 27; }
enter() { key 13; }

turn_count() {
    grep -c "user" chats/HORN_SESSIONS/chat_history.txt 2>/dev/null || echo 0
}
# True once the cli_io is actually accepting keystrokes. Typing before this
# lands in nav mode, where letters are navigation commands - that is how a
# test ends up sending half a path and the other half as a message.
wait_typing() {
    local i
    for (( i=0; i<40; i++ )); do
        [ "$(cat pieces/display/active_gui_is_typing.txt 2>/dev/null)" = "1" ] && return 0
        sleep 0.25
    done
    return 1
}
# Put focus in the composer and wait until it is really live, then empty it.
#
# ESC deliberately does NOT clear the box - chtpm's own handler keeps the
# buffer on deactivation ("KISS: ESC just deactivates, NEVER clears
# input"). So anything left from a previous step is still there and typing
# APPENDS to it, which silently produced doubled prompts like
# "What is 6 times 7?What is 6 times 7?". Backspace it out explicitly.
# Empty the composer and confirm it really is empty before returning.
#
# The backspaces are appended to the same history file the parser tails, so
# they are processed asynchronously. Returning as soon as they were WRITTEN
# races the parser: the next burst of typing lands before the clears do,
# and the prompt goes out with leftover characters - which showed up as
# "@ops/horn_t" being sent to the model as a chat turn instead of running
# completion.
# Empty the composer, one backspace at a time, waiting for each to land.
#
# Firing all the backspaces at once and then polling for an empty box does
# not work: gui_state is ALREADY empty in the moment right after a send
# (Enter clears it), so the poll returns instantly and the next burst of
# typing is queued BEHIND 200 undrained backspaces - which then erase it.
# That is how "@ops/horn_t" turned into a sent "t".
empty_composer() {
    local len i
    for (( len=0; len<300; len++ )); do
        len=$(sed -n 's/^horn_prompt=//p' pieces/apps/player_app/manager/gui_state.txt 2>/dev/null | head -1 | wc -c)
        [ "$len" -le 1 ] && return 0    # just the newline: box is empty
        printf 'KEY_PRESSED: 127\n' >> "$KEYS"
        for (( i=0; i<40; i++ )); do
            local now
            now=$(sed -n 's/^horn_prompt=//p' pieces/apps/player_app/manager/gui_state.txt 2>/dev/null | head -1 | wc -c)
            [ "$now" -lt "$len" ] && break
            sleep 0.15
        done
    done
    return 1
}
focus_composer() {
    esc; enter
    wait_typing || return 1
    empty_composer
}
# Send one message and wait for the turn to land. Free-tier models flake
# (rate limits, upstream 5xx), so a transport failure is retried rather
# than reported as a harness defect - but a retry re-sends the same
# question, so a genuinely broken transport still fails after N tries.
# Send one message and wait for the turn to land.
#
# Free-tier models flake (rate limits, upstream 5xx), so a transport failure
# is retried. But the DAILY quota (50 requests on this key) is not a flake:
# when it is spent, no model will answer for the rest of the UTC day and
# retrying only burns wall-clock. Detect it once, up front, and SKIP the
# live-API assertions rather than reporting a harness failure that is
# really an account state.
api_available() {
    # Judge by EXIT CODE, not by grepping the output. The first version
    # grepped for "quota" and the probe prompt itself was "quota probe",
    # so the model's own reply tripped the check and the live path was
    # skipped even with a working provider.
    #
    #   0 = a provider answered       10 = a provider answered WITH A TOOL
    #                                    CALL - still alive, see below
    #   1 = no key configured    2 = reachable but silent
    #   3 = everything rate/quota limited
    local out rc
    out=$(./ops/+x/horn_chat_backend.+x "Reply with the single word: ready" 2>&1)
    rc=$?
    echo "$out" >&2
    # 10 counts as available. Since tools were added, the model often
    # answers a probe by asking for one, and a check that only accepts 0
    # reported a perfectly healthy provider as unavailable - which silently
    # skipped every live assertion while the suite still printed a green
    # summary. A tool call is a response; it is not an outage.
    [ "$rc" -eq 0 ] || [ "$rc" -eq 10 ]
}

ask() {
    local msg="$1" want="$2" tries=${3:-3} i before
    for (( i=1; i<=tries; i++ )); do
        before=$(turn_count)
        focus_composer
        type_str "$msg"
        sleep 0.5
        enter
        for (( t=0; t<60; t++ )); do
            [ "$(turn_count)" -gt "$before" ] && break
            sleep 0.5
        done
        sleep 1
        if [ "$(turn_count)" -le "$before" ]; then
            echo "  ..turn $i: nothing dispatched, retrying"
        elif grep -qF "$want" "$TRANSCRIPT"; then
            return 0
        elif ! grep -q "no reply from model" "$TRANSCRIPT"; then
            return 0   # a real answer, just not the expected wording
        else
            echo "  ..turn $i: transport flake, backing off before retry"
            sleep 8
        fi
    done
    return 1
}

# Activate a nav item BY ITS INDEX, read from the live frame.
#
# House rules, from #.#.calendar-dox/1.^V-hq/_.0.aigent-testing-k9.txt
# (J2 Testing Guide). The two that bite:
#   1. Digits only work in NAV mode. While the cli_io is ACTIVE a digit is
#      just a character, so ESC out first or you are typing "3" into the
#      message. TAB does not move focus.
#   2. Nav indices are NOT stable ("Nav numbers are NOT fixed... no static
#      map you can hardcode"), so the index is read from the frame
#      immediately before use rather than assumed.
nav_focus_line() {
    grep -E '\[>\]' "$FRAME" | head -1 | sed 's/^ *//'
}
nav_item_index() {   # $1 = substring of the label, e.g. "DENY"
    local i idx
    # The frame repaints asynchronously, so read it until the label is
    # actually on screen. Reading once and failing looks identical to "the
    # button is not there", which is how this first reported a false miss.
    for (( i=0; i<20; i++ )); do
        idx=$(grep -E "\[[ >^]\] [0-9]+\..*$1" "$FRAME" 2>/dev/null | head -1 | grep -oE '[0-9]+\.' | head -1 | tr -d '.')
        [ -n "$idx" ] && { echo "$idx"; return 0; }
        sleep 0.4
    done
    return 1
}
# Press the nav number for a labelled item, then Enter to activate.
activate_nav_item() {
    local label="$1" idx
    esc                                  # leave the composer -> nav mode
    idx=$(nav_item_index "$label")
    if [ -z "$idx" ]; then
        bad "no nav item labelled '$label' in the frame"
        return 1
    fi
    printf 'KEY_PRESSED: %d\n' "$((48 + idx))" >> "$KEYS"
    sleep 0.6
    printf 'KEY_PRESSED: 13\n' >> "$KEYS"
    sleep 0.6
    return 0
}

wait_frame() {  # wait until the frame matches a pattern, or time out
    local pat="$1" tries=${2:-40} i
    for (( i=0; i<tries; i++ )); do
        if [ -f "$FRAME" ] && grep -qF "$pat" "$FRAME"; then return 0; fi
        sleep 0.5
    done
    return 1
}

cleanup() {
    # Match on argv TAILS, not on "$SCRIPT_DIR/system/...".
    #
    # The harness starts these with a cwd-relative path (./system/renderer),
    # so their /proc cmdline holds no project directory at all and a
    # $SCRIPT_DIR-prefixed pattern matches nothing. Earlier runs of this
    # script "passed" the no-leak assertion while leaving 30 orphaned
    # renderer/parser pairs behind - the pattern silently matched nothing
    # and the assertion checked a process that was already gone.
    #
    # pkill -f also takes an extended regex, so "prisc+x" would read the
    # '+' as a quantifier; match the .pal argument as a plain substring.
    pkill -9 -f "horn_main_loop.pal"          2>/dev/null
    pkill -9 -f "chtpm_parser_pal layouts"   2>/dev/null
    pkill -9 -f "renderer$"                  2>/dev/null
    sleep 0.5
}

# The pal module is a fork of the parser (the layout's <module> tag), so
# it must be matched by its .pal argument: killing the parser alone leaves
# it running.
boot() {
    cleanup
    printf 'horn_prompt=\n' > pieces/apps/player_app/manager/gui_state.txt
    : > "$KEYS"; : > pieces/apps/player_app/interact_relay.txt
    # Markers left over from a previous session lie about focus. An
    # active_gui_is_typing.txt still reading "1" makes wait_typing succeed
    # before the parser has focused anything, and every keystroke then goes
    # to nav mode as a navigation command instead of a character.
    printf '0' > pieces/display/active_gui_is_typing.txt
    ops/+x/horn_publish.+x
    nohup ./system/renderer          >/tmp/horn_render.log 2>&1 &
    nohup ./system/chtpm_parser_pal layouts/horn_chat.chtpm >/tmp/horn_parser.log 2>&1 &
    sleep 2.5
}

cleanup
[ "${1:-clean}" = "clean" ] && ./horn_chat.sh clean >/dev/null

echo "=== build ==="
bash scripts/build.sh >/tmp/horn_build.log 2>&1 || { echo "BUILD FAILED"; tail -20 /tmp/horn_build.log; exit 1; }
# A build that emits warnings from THIS project's own sources is a failed
# build for house purposes - the canonical shared-lib files are not ours
# to fix here.
if grep -E "^ops/|^system/" /tmp/horn_build.log | grep -q warning; then
    echo "BUILD FAILED: warnings in project sources"
    grep -E "^ops/|^system/" /tmp/horn_build.log | grep warning
    exit 1
fi
echo "  ok"

echo "=== boot ==="
boot

# The pal module is forked by the parser from the layout's <module> tag.
if pgrep -f "horn_main_loop.pal" >/dev/null; then ok "pal module forked by the parser"
else bad "pal module not running (check /tmp/horn_parser.log)"; fi

wait_frame "HORN_CHAT" 20 || bad "no first frame"
check "first frame renders the box" "$(cat "$FRAME")" "HORN_CHAT"
check "approval buttons are present" "$(cat "$FRAME")" "APPROVE"
check "model name resolved from state.txt" "$(cat "$FRAME")" "nemotron"

echo "=== turn 1: the composer holds what was typed ==="
# Use focus_composer, NOT a bare `key 13`. The parser drains the key
# history asynchronously, so a lone Enter can sit in the relay while the
# following characters are already being typed - and then be processed
# late, submitting the box and clearing it. Observed exactly that:
# type_str reported every character landed ("stalled: none"), and
# gui_state was empty at the assertion because the deferred Enter had
# already fired horn_turn. focus_composer is the same esc+enter+wait path
# ask() uses, so there is no unaccounted-for activation in flight.
if ! focus_composer; then bad "composer never became active"; fi
type_str "What is 6 times 7?"
# Assert on the FRAME, not gui_state: type_str already confirmed every
# character reached the parser (it polls gui_state, which chtpm rewrites
# per keystroke), and the frame repaints on its own cycle behind it.
if wait_frame "What is 6 times 7?" 20; then
    ok "typed text accumulates in the composer"
else
    bad "composer did not render the typed text [stalled: ${TYPE_STALLED:-none}] (gui_state had: $(cat pieces/apps/player_app/manager/gui_state.txt 2>/dev/null))"
fi
TYPE_STALLED=""

# Read-and-send in one step from here on. The separate pre-type above is
# only to prove the composer accumulates; sending it here too would double
# the text in the box, since Enter is what clears it.
echo "=== API availability ==="
# One probe decides whether the live round-trip assertions can run at all.
# Everything else below is offline and still runs either way.
API_UP=0
if api_available; then
    API_UP=1; ok "an LLM provider is answering"
else
    echo "  SKIP  no provider is answering (see the transport errors above)."
    echo "        Live round-trip assertions are skipped; the harness cannot"
    echo "        distinguish an exhausted quota from a broken transport"
    echo "        mid-run, so this is checked once, up front."
fi

if [ "$API_UP" = 1 ]; then
echo "=== turn 1: chat round-trip ==="
if ask "What is 6 times 7?" "42"; then ok "reply rendered (live LLM call)"
else bad "no model reply after 3 attempts"; fi
check "prompt is not doubled by the harness" "$(cat "$TRANSCRIPT")" "you: What is 6 times 7?"
# The transcript is written synchronously by horn_turn, but the FRAME is
# repainted asynchronously by the parser (state_changed marker -> compose
# -> renderer_pulse -> renderer). Wait for it rather than reading the file
# the instant the turn lands, or these assert on the previous frame.
wait_frame "turns: 1" 20 || bad "frame never showed the turn counter"
check "model reply shown" "$(cat "$FRAME")" "horn:"
check "turn counter advanced" "$(cat "$FRAME")" "turns: 1"
check "composer cleared after send" "$(cat pieces/apps/player_app/manager/gui_state.txt)" "horn_prompt="
check "history is persistent TSV" "$(cat chats/HORN_SESSIONS/chat_history.txt)" "user	What is 6 times 7?"

echo "=== turn 2: second turn proves the loop keeps running ==="
if ask "Name a primary color." "Red"; then ok "second turn dispatched"
else bad "second turn did not produce a reply"; fi
# Assert on the transcript, not the frame: the frame is a fixed-height box
# and a long earlier reply legitimately pushes older lines out of view.
check "transcript keeps both turns" "$(cat "$TRANSCRIPT")" "you: What is 6 times 7?"
check "second turn recorded"        "$(cat "$TRANSCRIPT")" "you: Name a primary color."
else
  echo "--- skipping live chat turns; verifying the offline paths instead ---"
  # Even with no API, the turn pipeline must still run end to end: the
  # prompt is consumed, the box clears, history and transcript record it,
  # and the failure is reported rather than silently dropped.
  before=$(turn_count)
  focus_composer || bad "composer not available"
  type_str "offline pipeline check"
  enter
  sleep 4
  [ "$(turn_count)" -gt "$before" ] && ok "turn dispatched without a usable API" \
                                   || bad "turn not dispatched"
  check "failure is recorded, not dropped" "$(cat "$TRANSCRIPT")" "you: offline pipeline check"
  check "composer still cleared" "$(cat pieces/apps/player_app/manager/gui_state.txt)" "horn_prompt="
fi

echo "=== '@' completion (offline: no API needed) ==="
before=$(turn_count)
focus_composer || bad "composer not available for the completion test"
type_str "@ops/horn_t"
sleep 0.8
check "the completion request is staged in the box" "$(cat "$FRAME")" "@ops/horn_t"
enter
if wait_frame "path: ops/horn_turn.c" 20; then ok "completion listing rendered"
else bad "completion listing missing"; fi
if grep -q "path: ops/horn_turn.c" "$TRANSCRIPT"; then
    ok "listing recorded in the transcript"
else bad "listing not in the transcript"; fi
# One assertion covers both "did not go to the model" and "cost no turn":
# a completion that reached a provider would necessarily add a user turn.
# Counting turns is also robust to wording - an earlier version matched a
# literal "horn: 42" and failed the moment a provider answered
# "6 x 7 = 42" instead of a bare number.
[ "$(turn_count)" = "$before" ] && ok "completion did not go to the model, and cost no turn" \
                               || bad "completion was sent as a chat turn"

# ── tools ──────────────────────────────────────────────────────────────
# These run whether or not the API is up: horn_tool_exec has no network in
# it. The live model-driven tool test is below and is skipped without one.
TOOL=ops/+x/horn_tool_exec.+x

echo "=== tool allowlist (offline) ==="
out=$($TOOL list_dir '{"path":"ops"}' 2>&1)
case "$out" in
  *"horn_turn.c"*) ok "list_dir lists the project" ;;
  *) bad "list_dir returned nothing useful: ${out:0:80}" ;;
esac

out=$($TOOL read_file '{"path":"tools/horn_tools.json","max_bytes":400}' 2>&1)
case "$out" in
  *horn_tools.json*|*HORN*) ok "read_file returns file contents" ;;
  *) bad "read_file failed: ${out:0:80}" ;;
esac

out=$($TOOL grep_files '{"pattern":"MAX_TOOL_ROUNDS","path":"ops/horn_turn.c"}' 2>&1)
case "$out" in
  *"#define MAX_TOOL_ROUNDS 6"*) ok "grep_files greps a FILE path" ;;
  *) bad "grep_files on a file path failed: ${out:0:90}" ;;
esac

# The allowlist is the security boundary: a tool the model invents must be
# refused, not dispatched.
out=$($TOOL definitely_not_a_real_tool '{"path":"/"}' 2>&1)
case "$out" in
  *"not registered"*) ok "unregistered tool is refused" ;;
  *) bad "UNREGISTERED TOOL WAS NOT REFUSED: ${out:0:90}" ;;
esac

# Runtime trees hold this session's own transcript; grepping them returns
# the model's own question echoed back, which it then reports as a finding.
out=$($TOOL grep_files '{"pattern":"tool","path":"chats"}' 2>&1)
case "$out" in
  *"refusing to search"*) ok "grep refuses the session transcript" ;;
  *) bad "grep searched the transcript: ${out:0:90}" ;;
esac

out=$($TOOL read_file '{"path":"/etc/shadow"}' 2>&1)
case "$out" in
  *"error:"*) ok "unreadable file returns an error, not a crash" ;;
  *) bad "read_file on /etc/shadow: ${out:0:90}" ;;
esac

echo "=== live tool use through the model (live API only) ==="
if [ "$API_UP" = 1 ]; then
    rm -f chats/HORN_SESSIONS/transcript.txt
    ./horn_chat.sh send "Use grep_files to find where MAX_TOOL_ROUNDS is defined in ops/horn_turn.c, then tell me the number." >/dev/null 2>&1
    if grep -q "tool: grep_files" "$TRANSCRIPT"; then
        ok "model invoked grep_files through the loop"
    else
        bad "model did not use a tool: $(tail -2 "$TRANSCRIPT" | tr '\n' ' ' | cut -c1-90)"
    fi
    if grep -q "#define MAX_TOOL_ROUNDS 6" "$TRANSCRIPT"; then
        ok "tool result fed back to the model and used in its answer"
    else
        bad "tool result never reached the model"
    fi
    # A tool-using turn must still end with a plain answer, not a loop.
    if grep -q "horn: " "$TRANSCRIPT" && ! grep -q "stopped:" "$TRANSCRIPT"; then
        ok "tool turn terminated with an answer"
    else
        bad "tool turn did not terminate cleanly"
    fi
else
    echo "  SKIP  model-driven tool test needs a live provider"
fi

# ── write / exec containment (offline: no LLM, no network needed) ────
echo "=== write containment (offline) ==="
mkdir -p config
printf 'alpha\nbeta\nbeta\n' > dox/.e2e_edit.txt
TOOL="$TOOL"
: > config/yolo.flag          # arm so direct op calls are not gated anyway

out=$($TOOL edit_file '{"path":"dox/.e2e_edit.txt","search":"beta","replace":"BETA"}' 2>&1)
case "$out" in
  *"appears 2 times"*) ok "edit_file refuses an ambiguous match" ;;
  *) bad "edit_file did not refuse ambiguity: ${out:0:80}" ;;
esac
out=$($TOOL edit_file '{"path":"dox/.e2e_edit.txt","search":"beta","replace":"X","replace_all":1}' 2>&1)
case "$out" in
  *"2 replacements"*) ok "edit_file honours replace_all" ;;
  *) bad "edit_file replace_all failed: ${out:0:80}" ;;
esac
out=$($TOOL edit_file '{"path":"dox/.e2e_edit.txt","search":"absent","replace":"y"}' 2>&1)
case "$out" in
  *"not found"*) ok "edit_file refuses a missing match, changes nothing" ;;
  *) bad "edit_file did not report a missing match: ${out:0:80}" ;;
esac
rm -f dox/.e2e_edit.txt

printf '# original\n' > dox/.e2e_w.txt
out=$($TOOL write_file '{"path":"dox/.e2e_w.txt","content":"# overwritten\n"}' 2>&1)
case "$out" in
  *"wrote"*) ok "write_file overwrites an existing file" ;;
  *) bad "write_file failed on an existing file: ${out:0:80}" ;;
esac
if [ "$(cat dox/.e2e_w.txt)" = "# overwritten" ]; then ok "write landed on disk"
else bad "write_file did not change the file"; fi

# The allowlist is writable by a human but must NOT be writable by a model,
# or the gate list next to it stops meaning anything.
out=$($TOOL write_file '{"path":"tools/horn_tools.json","content":"x"}' 2>&1)
case "$out" in
  *"protected path"*) ok "write_file refuses the tool allowlist itself" ;;
  *) bad "MODEL COULD WRITE THE ALLOWLIST: ${out:0:80}" ;;
esac
out=$($TOOL write_file '{"path":"dox/../../../../tmp/e2e-escape","content":"x"}' 2>&1)
case "$out" in
  *"outside the project root"*) ok "write_file refuses '..' escapes" ;;
  *) bad "write_file allowed a '..' escape: ${out:0:80}" ;;
esac
out=$($TOOL write_file '{"path":"/etc/e2e","content":"x"}' 2>&1)
case "$out" in
  *"outside the project root"*) ok "write_file refuses absolute paths" ;;
  *) bad "write_file allowed an absolute path: ${out:0:80}" ;;
esac
out=$($TOOL write_file '{"path":"config/raw_groq.txt","content":"x"}' 2>&1)
case "$out" in
  *"protected path"*) ok "write_file refuses key-shaped files" ;;
  *) bad "write_file would write a key file: ${out:0:80}" ;;
esac
out=$($TOOL write_file '{"path":"no/such/dir/x.txt","content":"x"}' 2>&1)
case "$out" in
  *"does not exist"*) ok "write_file will not create directories" ;;
  *) bad "write_file created a directory tree: ${out:0:80}" ;;
esac
rm -f dox/.e2e_w.txt

echo "=== script sandbox (offline) ==="
out=$($TOOL run_script '{"command":"echo sandbox-ok"}' 2>&1)
case "$out" in
  *sandbox-ok*) ok "run_script runs a normal command" ;;
  *) bad "run_script failed on a trivial command: ${out:0:90}" ;;
esac
out=$($TOOL run_script '{"command":"touch ../e2e-escape.txt; echo rc=$?"}' 2>&1)
case "$out" in
  *"rc=1"*|*"Read-only"*) ok "sandbox blocks writes outside the project" ;;
  *) bad "sandbox ALLOWED a parent-directory write: ${out:0:90}" ;;
esac
[ -f ../e2e-escape.txt ] && { bad "escape file exists on the real filesystem"; rm -f ../e2e-escape.txt; } \
                       || ok "no escape file leaked to the real filesystem"
out=$($TOOL run_script '{"command":"touch /etc/e2e-horn 2>&1 | head -1; echo rc=$?"}' 2>&1)
case "$out" in
  *"Read-only"*) ok "sandbox blocks writes to /etc" ;;
  *) bad "sandbox may have written to /etc: ${out:0:90}" ;;
esac
out=$($TOOL run_script '{"command":"getent hosts openrouter.ai >/dev/null 2>&1 && echo DNS-UP || echo DNS-BLOCKED"}' 2>&1)
case "$out" in
  *DNS-BLOCKED*) ok "sandbox blocks the network" ;;
  *) bad "sandbox ALLOWED network access: ${out:0:90}" ;;
esac
# head -c 200, not 30: the point is to capture the whole
# "No such file or directory" and 30 chars cut it off mid-path, which
# looked exactly like the keys being reachable.
out=$($TOOL run_script '{"command":"cat \"../&.widgits/open-hai/state/raw_groq.txt\" 2>&1 | head -c 200"}' 2>&1)
case "$out" in
  *"No such file"*) ok "sandbox hides the provider keys" ;;
  *) bad "KEYS REACHABLE FROM THE SANDBOX: ${out:0:90}" ;;
esac
rm -f config/yolo.flag

echo "=== approval gate through the real UI (live) ==="
# Each gate outcome is proven from a CLEAN session: teardown, boot, drive.
# Testing deny-then-approve back to back in one window worked until it
# aborted the suite partway through the approve case, and a gate test that
# only proves itself in one particular accumulated state is a weaker test
# anyway. This is also what the house J2 guide asks for: kill everything,
# confirm zero, launch one, confirm one.
gate_case() {   # $1 = DENY|APPROVE
    # Only $1: an unused "$2" here is fatal under `set -u`, which is what
    # ended the previous run at this line with EXIT=0 and no summary.
    local want="$1"
    cleanup
    printf '# original\n' > dox/.e2e_gate.md
    boot

    focus_composer
    type_str "Overwrite dox/.e2e_gate.md to contain exactly $want"
    enter
    if wait_frame "write_file" 45; then ok "$want: gated tool stops and asks"
    else bad "$want: gate never prompted"; return 1; fi

    if ! activate_nav_item "$want"; then bad "$want: could not find the $want button"; return 1; fi
    if wait_frame "DENIED by user" 30; then ok "$want: button registers through the UI"
    else bad "$want: button did not register"; fi

    if [ "$want" = "DENY" ]; then
        if [ "$(cat dox/.e2e_gate.md)" = "# original" ]; then
            ok "denied write left the file byte-identical"
        else
            bad "DENIED BUT THE FILE CHANGED: $(cat dox/.e2e_gate.md)"
        fi
    else
        if [ "$(cat dox/.e2e_gate.md)" = "# original" ]; then
            bad "APPROVED BUT THE FILE DID NOT CHANGE"
        else
            ok "approved write actually landed on disk"
        fi
    fi
    return 0
}

if [ "$API_UP" = 1 ]; then
    # Deny first. A deny-only test cannot tell a working gate from one
    # that always denies, which is the failure that would matter.
    gate_case DENY
    gate_case APPROVE
    cleanup
    rm -f dox/.e2e_gate.md
else
    echo "  SKIP  approval-gate UI test needs a live provider"
fi

echo "=== persistence across a restart ==="
# Use whatever the session actually recorded. In the offline branch that is
# the failed pipeline check, not the 6-times-7 prompt.
persisted="$(grep -m1 'user' "$TRANSCRIPT" | cut -f3)"
before=$(turn_count)
sleep 1
boot   # clean relay, so this specifically proves history persists
if wait_frame "$persisted" 20; then ok "history survives a restart"
else bad "history lost on restart (expected to see '$persisted')"; fi
after=$(grep -c "user" chats/HORN_SESSIONS/chat_history.txt)
[ "$before" = "$after" ] && ok "restart added no phantom turns" \
                         || bad "restart changed history ($before -> $after turns)"

echo "=== a stale relay must not replay into a new session ==="
# The relay file is append-only across sessions. A pal loop that starts
# its cursor at 0 replays every Enter the user ever pressed, re-dispatching
# horn_turn against the CURRENT gui_state - phantom turns in the transcript
# and duplicated history. Guard: seed the cursor at end-of-file.
# Clear the composer: a leftover prompt would make a stray Enter dispatch
# a REAL turn, and the suite would blame the replay for its own leftover.
printf 'horn_prompt=\n' > pieces/apps/player_app/manager/gui_state.txt
before=$(grep -c "user" chats/HORN_SESSIONS/chat_history.txt)
printf '13\n13\n13\n' >> pieces/apps/player_app/interact_relay.txt   # stale, pre-seeded
boot_keep_relay() {
    cleanup
    printf 'horn_prompt=\n' > pieces/apps/player_app/manager/gui_state.txt
    : > "$KEYS"
    ops/+x/horn_publish.+x
    nohup ./system/renderer          >/tmp/horn_render3.log 2>&1 &
    nohup ./system/chtpm_parser_pal layouts/horn_chat.chtpm >/tmp/horn_parser3.log 2>&1 &
    sleep 3
}
boot_keep_relay
sleep 2
after=$(grep -c "user" chats/HORN_SESSIONS/chat_history.txt)
[ "$before" = "$after" ] && ok "stale relay Enters did not replay" \
                         || bad "stale relay replayed ($before -> $after turns)"

echo "=== teardown ==="
cleanup
# Count survivors and SHOW them: a bare pass/fail here previously reported
# success while orphans were still running, because the check and the kill
# were matching the same wrong pattern.
leaks=$(pgrep -af "horn_main_loop.pal|chtpm_parser_pal|renderer$" 2>/dev/null | grep -v pgrep || true)
if [ -n "$leaks" ]; then
    bad "processes leaked after teardown:"
    printf '%s\n' "$leaks" | sed 's/^/         /'
else
    ok "no leaked processes"
fi

echo
echo "=== $pass passed, $fail failed ==="
[ "$fail" -eq 0 ] || exit 1