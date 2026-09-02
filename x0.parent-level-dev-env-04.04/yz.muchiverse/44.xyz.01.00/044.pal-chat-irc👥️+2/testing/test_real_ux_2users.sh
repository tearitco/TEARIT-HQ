#!/bin/bash
# testing/test_real_ux_2users.sh - drives TWO real users through the
# ACTUAL keyboard/menu UX (real key injection into
# pieces/keyboard/history.txt, real login/signup/room-join/type-message
# flow through chat_menu_input.c's own numbered-item dispatch), not
# direct op invocation. See #.haiku+/!.local-ux-testing-ai.txt for the
# full writeup of how this interaction model was reverse-engineered
# live and why testing/test_multiuser_p2p.sh (direct op calls) is NOT
# a substitute for this.
#
# CPU SAFETY: this script is intentionally short-lived and ALWAYS cleans
# up via trap, verified independently (not trusted from exit codes) -
# see #.haiku+/!.xyzos-pitfalls+1.txt PITFALL 20/21/22 for why that
# matters here specifically (a real, now-fixed CPU-spin bug in
# system/keyboard_input.c, and kill_all.sh's own unreliable one-shot
# completion). Run this under `timeout` yourself if you want an extra
# outer safety net: `timeout 120 bash testing/test_real_ux_2users.sh`
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

PROOF_DIR="$SCRIPT_DIR/proof/ux-$(date +%Y%m%d-%H%M%S)"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

cleanup() {
    echo
    echo "--- cleanup ---"
    bash "$SCRIPT_DIR/pieces/os/kill_all.sh" >/dev/null 2>&1
    sleep 1
    LEFTOVER=$(ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|ops/\+x/(palnet_peer|chat_inbox_watcher)|prisc\+x" | grep -v grep)
    if [ -n "$LEFTOVER" ]; then
        echo "$LEFTOVER" | awk '{print $2}' | xargs -r kill -9
        sleep 1
    fi
    STILL=$(ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|ops/\+x/(palnet_peer|chat_inbox_watcher)|prisc\+x" | grep -v grep)
    if [ -n "$STILL" ]; then
        fail "cleanup - processes still running"
        echo "$STILL"
    else
        pass "cleanup - no lingering processes (verified via ps)"
    fi
}
trap cleanup EXIT

# --- key injection helpers (see !.local-ux-testing-ai.txt for the model) ---
inject_key() {
    local hist="$1" code="$2"
    echo "[2026-07-26 16:00:00] KEY_PRESSED: $code" >> "$hist"
    sleep 0.3
}
type_text() {
    local hist="$1" text="$2"
    local i ch code
    for (( i=0; i<${#text}; i++ )); do
        ch="${text:$i:1}"
        code=$(printf '%d' "'$ch")
        echo "[2026-07-26 16:00:00] KEY_PRESSED: $code" >> "$hist"
        sleep 0.08
    done
}
# Discover a numbered menu item's CURRENT number by its label text -
# item numbers SHIFT depending on screen state (how many rooms exist,
# whether logged_in is true yet, etc.) so never hardcode them.
item_num() {
    local frame="$1" label="$2"
    grep -oP '\]\s*\K[0-9]+(?=\.\s*\[?'"$label"')' "$frame" | head -1
}

echo "=== pal-chat-irc REAL key-injected 2-user UX test ==="
mkdir -p "$PROOF_DIR"
bash "$SCRIPT_DIR/pieces/os/kill_all.sh" >/dev/null 2>&1
sleep 1

echo "--- launching two real sessions via ./button.sh run (real UX entry point) ---"
NO_GL=1 setsid bash button.sh run --pal > /tmp/ux_sess_a.log 2>&1 < /dev/null &
disown
sleep 1
NO_GL=1 setsid bash button.sh run --pal > /tmp/ux_sess_b.log 2>&1 < /dev/null &
disown
sleep 3

SESS_A=$(ls -dt "$SCRIPT_DIR"/pieces/sessions/*/ | sed -n '2p'); SESS_A="${SESS_A%/}"
SESS_B=$(ls -dt "$SCRIPT_DIR"/pieces/sessions/*/ | sed -n '1p'); SESS_B="${SESS_B%/}"
if [ -z "$SESS_A" ] || [ -z "$SESS_B" ]; then
    fail "session launch"; exit 1
fi
echo "Session A (alice): $SESS_A"
echo "Session B (bob):   $SESS_B"

HIST_A="$SESS_A/pieces/keyboard/history.txt"
FRAME_A="$SESS_A/pieces/display/current_frame.txt"
HIST_B="$SESS_B/pieces/keyboard/history.txt"
FRAME_B="$SESS_B/pieces/display/current_frame.txt"
ROOM="uxtest_$$"
USER_A="alice_$$"
USER_B="bob_$$"

# NEVER assume a fixed item number for anything, even "item 1" on a
# freshly-loaded screen - nav focus can restore to a previously-focused
# element (observed live: room_list's default focus landed on an
# existing room in the list, not on the Room Name field, on a session
# that had never touched that screen before). Always focus explicitly
# by looked-up item number first.
focus_item() {
    local hist="$1" frame="$2" label="$3"
    local n; n=$(item_num "$frame" "$label")
    if [ -z "$n" ]; then echo "  (focus_item: '$label' not found in frame)" >&2; return 1; fi
    # Nav-mode digit accumulation (chtpm_parser_pal.c ~line 3064-3095):
    # typing '1' then '0' jumps to item 1, THEN re-accumulates to item 10
    # (do_jump() fires on EVERY valid intermediate value, not just the
    # final one) - real single-digit items (1-9) need exactly one
    # keypress, real multi-digit items (10+) need one digit keypress per
    # character, injected as SEPARATE keystrokes, exactly as a human
    # typing that number would produce. type_text already does this
    # (one KEY_PRESSED line per character of the string).
    type_text "$hist" "$n"
}

login_flow() {
    local hist="$1" frame="$2" userid="$3" dispname="$4"
    focus_item "$hist" "$frame" "User ID"
    inject_key "$hist" 13           # Enter: activate the now-focused field
    type_text "$hist" "$userid"
    inject_key "$hist" 27           # ESC: back to nav
    focus_item "$hist" "$frame" "Display Name"
    inject_key "$hist" 13           # Enter: activate it
    type_text "$hist" "$dispname"
    inject_key "$hist" 27           # ESC: back to nav
    focus_item "$hist" "$frame" "Create Account"
    inject_key "$hist" 13           # Enter: execute SIGNUP (auto-logs in)
    sleep 0.5
    focus_item "$hist" "$frame" "Continue to Rooms"
    inject_key "$hist" 13           # Enter: href to room_list
}

join_room_flow() {
    local hist="$1" frame="$2" room="$3"
    focus_item "$hist" "$frame" "Room Name"
    inject_key "$hist" 13           # Enter: activate the now-focused field
    type_text "$hist" "$room"
    inject_key "$hist" 27           # ESC
    focus_item "$hist" "$frame" "Join / Create Room"
    inject_key "$hist" 13           # Enter: execute JOIN_ROOM
    sleep 0.5
    focus_item "$hist" "$frame" "Enter Room"
    inject_key "$hist" 13           # Enter: href into room.chtpm
}

send_message() {
    local hist="$1" text="$2"
    inject_key "$hist" 13           # Enter: activate Message field
    type_text "$hist" "$text"
    inject_key "$hist" 13           # Enter again: SEND (stays active, no ESC)
}

echo
echo "--- alice: signup + login + create room '$ROOM' ---"
login_flow "$HIST_A" "$FRAME_A" "$USER_A" "Alice"
sleep 1
if grep -q "\[room_list\]" "$FRAME_A"; then pass "alice reached room_list via real signup+login"; else fail "alice did not reach room_list"; fi
join_room_flow "$HIST_A" "$FRAME_A" "$ROOM"
sleep 1
if grep -q "Room: $ROOM (as $USER_A)" "$FRAME_A"; then pass "alice joined room '$ROOM' via real key input"; else fail "alice did not enter room '$ROOM'"; fi
cp "$FRAME_A" "$PROOF_DIR/alice_in_room.txt"

echo
echo "--- bob: signup + login + join SAME room '$ROOM' ---"
login_flow "$HIST_B" "$FRAME_B" "$USER_B" "Bob"
sleep 1
if grep -q "\[room_list\]" "$FRAME_B"; then pass "bob reached room_list via real signup+login"; else fail "bob did not reach room_list"; fi
join_room_flow "$HIST_B" "$FRAME_B" "$ROOM"
sleep 1
if grep -q "Room: $ROOM (as $USER_B)" "$FRAME_B"; then pass "bob joined room '$ROOM' via real key input"; else fail "bob did not enter room '$ROOM'"; fi
cp "$FRAME_B" "$PROOF_DIR/bob_in_room.txt"

echo
echo "--- THE REAL TEST: alice types+sends a message, bob (idle, does nothing) should see it live ---"
send_message "$HIST_A" "hello bob, this is a real typed message"
sleep 3
cp "$FRAME_A" "$PROOF_DIR/alice_after_send1.txt"
cp "$FRAME_B" "$PROOF_DIR/bob_after_alice_send1.txt"
if grep -q "hello bob, this is a real typed message" "$FRAME_B"; then
    pass "bob's IDLE session updated live after alice's real keystroke - no action taken by bob"
else
    fail "bob's session did NOT show alice's message (P2P push/render-trigger not working end-to-end)"
fi

echo
echo "--- reverse: bob types+sends a reply, alice (idle) should see it live ---"
send_message "$HIST_B" "hi alice, replying for real"
sleep 3
cp "$FRAME_A" "$PROOF_DIR/alice_after_bob_reply.txt"
cp "$FRAME_B" "$PROOF_DIR/bob_after_send_reply.txt"
if grep -q "hi alice, replying for real" "$FRAME_A"; then
    pass "alice's IDLE session updated live after bob's real keystroke"
else
    fail "alice's session did NOT show bob's reply"
fi

# save full frame histories regardless of outcome
cp "$SESS_A/debug/frame_history.txt" "$PROOF_DIR/alice_frame_history.txt" 2>/dev/null
cp "$SESS_B/debug/frame_history.txt" "$PROOF_DIR/bob_frame_history.txt" 2>/dev/null
rm -rf "$SCRIPT_DIR/rooms/$ROOM"

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then
    echo "=== OVERALL: FAIL - see above ==="
    exit 1
else
    echo "=== OVERALL: PASS ==="
    exit 0
fi
