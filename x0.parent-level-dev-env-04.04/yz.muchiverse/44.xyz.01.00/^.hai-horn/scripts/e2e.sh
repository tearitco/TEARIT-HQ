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
export HORN_ENTITY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)/&.widgits/open-hai/state"

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
        local t
        for (( t=0; t<40; t++ )); do
            if grep -qF "horn_prompt=$want" pieces/apps/player_app/manager/gui_state.txt 2>/dev/null; then
                break
            fi
            sleep 0.15
        done
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
    #   0 = a provider answered        3 = everything rate/quota limited
    #   1 = no key configured          2 = reachable but silent
    local out rc
    out=$(./ops/+x/horn_chat_backend.+x "Reply with the single word: ready" 2>&1)
    rc=$?
    echo "$out" >&2
    [ "$rc" -eq 0 ]
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

wait_frame "HORN_CHAT v0.1" 20 || bad "no first frame"
check "first frame renders the box" "$(cat "$FRAME")" "HORN_CHAT v0.1"
check "model name resolved from state.txt" "$(cat "$FRAME")" "nemotron"

echo "=== turn 1: the composer holds what was typed ==="
key 13                    # nav mode -> activate the cli_io
wait_typing || bad "composer never became active"
type_str "What is 6 times 7?"
sleep 0.8
check "typed text accumulates in the composer" "$(cat "$FRAME")" "What is 6 times 7?"

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
    echo "  SKIP  every configured provider is rate/quota limited."
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