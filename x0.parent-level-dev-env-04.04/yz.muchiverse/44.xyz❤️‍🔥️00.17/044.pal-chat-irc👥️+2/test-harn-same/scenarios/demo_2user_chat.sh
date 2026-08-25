#!/bin/bash
# demo_2user_chat.sh - reference SCENARIO built from test-harn-same/ops/.
# This file only SEQUENCES calls to the ops - it holds no key-injection
# logic of its own (that all lives in ops/tk_*.c). Copy this file as a
# starting point for a different scenario; the ops themselves are
# scenario-agnostic and reusable as-is.
#
# Proves: two real users, via real signup/login/room-join/typed-message
# flow (not direct op-level posting), each see the OTHER's message live
# with zero action taken on their own side - the actual thing that was
# found broken in #.haiku+/!.xyzos-pitfalls+1.txt PITFALL 20.
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
OPS="$HARNESS_DIR/ops/+x"

PROOF_DIR="$PROJECT_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0

cleanup() {
    echo; echo "--- cleanup ---"
    bash "$HARNESS_DIR/button.sh" kill
}
trap cleanup EXIT

# --- thin wrappers around the ops - one line each, all the real logic
# lives in the C binaries themselves ---
key()    { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.2; }
type_()  { "$OPS/tk_type_text.+x" "$1" "$2"; sleep 0.2; }
focus()  { "$OPS/tk_focus_item.+x" "$1" "$2" "$3" >/dev/null; sleep 0.2; }
check()  { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }

login_flow() {
    local sess="$1" frame="$2" userid="$3" dispname="$4"
    focus "$sess" "$frame" "User ID";        key "$sess" 13; type_ "$sess" "$userid";   key "$sess" 27
    focus "$sess" "$frame" "Display Name";   key "$sess" 13; type_ "$sess" "$dispname"; key "$sess" 27
    focus "$sess" "$frame" "Create Account"; key "$sess" 13; sleep 0.5
    focus "$sess" "$frame" "Continue to Rooms"; key "$sess" 13
}

join_room_flow() {
    local sess="$1" frame="$2" room="$3"
    focus "$sess" "$frame" "Room Name"; key "$sess" 13; type_ "$sess" "$room"; key "$sess" 27
    focus "$sess" "$frame" "Join / Create Room"; key "$sess" 13; sleep 0.5
    focus "$sess" "$frame" "Enter Room"; key "$sess" 13
}

send_message() {
    local sess="$1" text="$2"
    key "$sess" 13; type_ "$sess" "$text"; key "$sess" 13   # 2nd Enter = send, stays active
}

echo "=== demo_2user_chat: real key-injected 2-user UX scenario ==="
bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
sleep 1

cd "$PROJECT_DIR"
NO_GL=1 setsid bash button.sh run --pal > /tmp/th_sess_a.log 2>&1 < /dev/null & disown
sleep 1
NO_GL=1 setsid bash button.sh run --pal > /tmp/th_sess_b.log 2>&1 < /dev/null & disown
sleep 3

SESS_A=$(ls -dt pieces/sessions/*/ | sed -n '2p'); SESS_A="${SESS_A%/}"
SESS_B=$(ls -dt pieces/sessions/*/ | sed -n '1p'); SESS_B="${SESS_B%/}"
FRAME_A="$SESS_A/pieces/display/current_frame.txt"
FRAME_B="$SESS_B/pieces/display/current_frame.txt"
ROOM="harness_room_$$"
USER_A="halice_$$"
USER_B="hbob_$$"

echo "--- alice ($USER_A): signup, login, create/join room '$ROOM' ---"
login_flow "$SESS_A" "$FRAME_A" "$USER_A" "HAlice"
sleep 1
check "$FRAME_A" "[room_list]" "alice reached room_list"
join_room_flow "$SESS_A" "$FRAME_A" "$ROOM"
sleep 1
check "$FRAME_A" "Room: $ROOM (as $USER_A)" "alice joined room '$ROOM'"
cp "$FRAME_A" "$PROOF_DIR/alice_in_room.txt"

echo "--- bob ($USER_B): signup, login, join SAME room '$ROOM' ---"
login_flow "$SESS_B" "$FRAME_B" "$USER_B" "HBob"
sleep 1
check "$FRAME_B" "[room_list]" "bob reached room_list"
join_room_flow "$SESS_B" "$FRAME_B" "$ROOM"
sleep 1
check "$FRAME_B" "Room: $ROOM (as $USER_B)" "bob joined room '$ROOM'"
cp "$FRAME_B" "$PROOF_DIR/bob_in_room.txt"

echo "--- alice sends, bob (idle) should see it live ---"
send_message "$SESS_A" "hello from the ops-based test harness"
sleep 3
cp "$FRAME_B" "$PROOF_DIR/bob_after_alice_send.txt"
check "$FRAME_B" "hello from the ops-based test harness" "bob's idle session updated live after alice's real keystrokes"

echo "--- bob replies, alice (idle) should see it live ---"
send_message "$SESS_B" "reply via the ops-based harness"
sleep 3
cp "$FRAME_A" "$PROOF_DIR/alice_after_bob_reply.txt"
check "$FRAME_A" "reply via the ops-based harness" "alice's idle session updated live after bob's real keystrokes"

rm -rf "$PROJECT_DIR/rooms/$ROOM"
echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
