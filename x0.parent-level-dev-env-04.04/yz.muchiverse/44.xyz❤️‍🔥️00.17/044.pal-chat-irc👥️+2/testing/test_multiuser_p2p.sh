#!/bin/bash
# testing/test_multiuser_p2p.sh - regression check for pal-chat-irc's
# multi-session P2P layer (the orchestrator -> palnet_peer -> net/inbox.txt
# -> chat_inbox_watcher -> trigger_render() chain).
#
# WHY THIS SCRIPT EXISTS: this exact chain was found COMPLETELY BROKEN
# on 2026-07-26 (see #.haiku+/!.xyzos-pitfalls+1.txt PITFALL 20) - the
# orchestrator's launch_redirect() call for palnet_peer collapsed 5
# required args into 1 string, so palnet_peer exited on its own usage
# message every single time, silently, into a log file nobody checked.
# The bug was invisible to op-level tests AND to isolated-two-node
# network tests (see PITFALL 21) because rooms/*/messages.txt and
# data/master_ledger.txt are REAL, SHARED files across sessions of the
# SAME local project - any session redrawing for its own reasons (its
# own keypress) picks up shared data directly, with zero help from P2P,
# giving false confidence that messaging "works" when it's actually
# only ever the filesystem doing the work, never the network layer.
#
# This script closes that gap by testing through the ACTUAL entry point
# real users use (./button.sh run, twice concurrently) and asserting on
# the specific things that were silently broken: port allocation via
# net/presence/, and message delivery via net/inbox.txt.
#
# USAGE: bash testing/test_multiuser_p2p.sh
# (run from anywhere - it cd's to the project root itself)
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

PRESENCE_DIR="$(dirname "$SCRIPT_DIR")/net/presence"
PROOF_DIR="$SCRIPT_DIR/proof/$(date +%Y%m%d-%H%M%S)"
FAIL=0

pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

cleanup() {
    echo
    echo "--- cleanup ---"
    bash "$SCRIPT_DIR/pieces/os/kill_all.sh" >/dev/null 2>&1
    # PITFALL 21: button.sh kill / kill_all.sh has been observed to not
    # always finish in one shot under CPU load - always verify, don't
    # trust the exit code, and follow up with a direct kill by PID.
    sleep 1
    # NOTE (found during this script's own first dogfood run, 2026-07-26):
    # daemons launched via a relative path (ops/+x/palnet_peer.+x) show up
    # in `ps aux` WITHOUT the project's absolute path in their cmdline - a
    # grep requiring "044.pal-chat-irc" in the same match as the process
    # name will silently miss them. Match on process identity alone.
    LEFTOVER=$(ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|ops/\+x/(palnet_peer|chat_inbox_watcher)|prisc\+x" | grep -v grep)
    if [ -n "$LEFTOVER" ]; then
        echo "kill_all.sh left processes behind, force-killing by PID:"
        echo "$LEFTOVER"
        echo "$LEFTOVER" | awk '{print $2}' | xargs -r kill -9
        sleep 1
    fi
    STILL=$(ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|ops/\+x/(palnet_peer|chat_inbox_watcher)|prisc\+x" | grep -v grep)
    if [ -n "$STILL" ]; then
        fail "cleanup - processes still running after kill_all.sh + manual kill -9"
        echo "$STILL"
    else
        pass "cleanup - no lingering processes (verified via ps, not assumed from exit code)"
    fi
}
trap cleanup EXIT

echo "=== pal-chat-irc multi-session P2P regression test ==="
echo "Project: $SCRIPT_DIR"
echo

# --- pre-flight: make sure nothing stale is running or registered ---
bash "$SCRIPT_DIR/pieces/os/kill_all.sh" >/dev/null 2>&1
rm -f "$PRESENCE_DIR"/*.txt 2>/dev/null
mkdir -p "$PROOF_DIR"

echo "--- launching two real sessions via ./button.sh run (the actual user entry point) ---"
NO_GL=1 bash button.sh run --pal > /tmp/test_p2p_sess_a.log 2>&1 &
sleep 1
NO_GL=1 bash button.sh run --pal > /tmp/test_p2p_sess_b.log 2>&1 &
sleep 3

SESS_A=$(ls -dt pieces/sessions/*/ 2>/dev/null | sed -n '2p')
SESS_B=$(ls -dt pieces/sessions/*/ 2>/dev/null | sed -n '1p')
if [ -z "$SESS_A" ] || [ -z "$SESS_B" ]; then
    fail "session launch - expected 2 new session dirs under pieces/sessions/, got fewer"
    exit 1
fi
echo "Session A: $SESS_A"
echo "Session B: $SESS_B"
echo

# --- CHECK 1: orchestrator actually launched a WORKING palnet_peer for
# each session (this is the exact thing PITFALL 20 broke - a fixed 2-arg
# launcher silently truncating palnet_peer's real argv). ---
echo "--- CHECK 1: port allocation (net/presence/) ---"
PORT_COUNT=$(ls "$PRESENCE_DIR"/*.txt 2>/dev/null | wc -l)
if [ "$PORT_COUNT" -lt 2 ]; then
    fail "port allocation - expected 2 presence files, found $PORT_COUNT (palnet_peer likely exited immediately - check /tmp/pal_chat_irc_palnet_peer.log for its own usage-error message, PITFALL 20)"
else
    PORTS=$(grep -h "^port=" "$PRESENCE_DIR"/*.txt | sort -u | wc -l)
    if [ "$PORTS" -lt 2 ]; then
        fail "port allocation - both sessions bound the SAME port (should be distinct via bind_with_retry)"
    else
        pass "port allocation - $PORT_COUNT presence files, $PORTS distinct port(s)"
    fi
fi
cp "$PRESENCE_DIR"/*.txt "$PROOF_DIR/" 2>/dev/null
echo

# --- CHECK 2: a message posted in session A actually reaches session
# B's net/inbox.txt (the actual thing that was silently dead). Uses
# direct op invocation for the POST action itself since chat_post_message.c
# was never the broken part - only the orchestrator's launch of
# palnet_peer was. Full real key-injected navigation is a separate,
# UI-level concern - see testing/README.txt. ---
echo "--- CHECK 2: cross-session message delivery via net/inbox.txt ---"
ROOM="p2p_test_room_$$"
PRISC_PROJECT_ROOT="$SCRIPT_DIR/$SESS_A" ops/+x/chat_post_message.+x "$ROOM" testUserA "p2p regression probe message" >/dev/null
sleep 3

if grep -q "$ROOM" "$SESS_B/net/inbox.txt" 2>/dev/null; then
    pass "cross-session delivery - session B's net/inbox.txt received session A's message"
else
    fail "cross-session delivery - session B's net/inbox.txt did NOT receive session A's message (net/inbox.txt content below)"
    echo "--- session B net/inbox.txt (actual content) ---"
    cat "$SESS_B/net/inbox.txt" 2>/dev/null
fi
cp "$SESS_A/net/outbox.txt" "$PROOF_DIR/sessA_outbox.txt" 2>/dev/null
cp "$SESS_B/net/inbox.txt" "$PROOF_DIR/sessB_inbox.txt" 2>/dev/null
echo

# --- CHECK 3: both files agree, no duplicate entries ---
echo "--- CHECK 3: room view + master ledger converged, no dupes ---"
A_MSGS=$(grep -c "^MSG" "rooms/$ROOM/messages.txt" 2>/dev/null || echo 0)
if [ "$A_MSGS" = "1" ]; then
    pass "room view - exactly 1 message in rooms/$ROOM/messages.txt (no dupes)"
else
    fail "room view - expected exactly 1 message, found $A_MSGS in rooms/$ROOM/messages.txt"
fi
LEDGER_MSGS=$(grep -c "|$ROOM|" "data/master_ledger.txt" 2>/dev/null || echo 0)
if [ "$LEDGER_MSGS" = "1" ]; then
    pass "master ledger - exactly 1 entry for this test room (no dupes)"
else
    fail "master ledger - expected exactly 1 entry for $ROOM, found $LEDGER_MSGS"
fi
echo

# --- save frame history proof from both sessions regardless of outcome ---
cp "$SESS_A/debug/frame_history.txt" "$PROOF_DIR/sessA_frame_history.txt" 2>/dev/null
cp "$SESS_B/debug/frame_history.txt" "$PROOF_DIR/sessB_frame_history.txt" 2>/dev/null
cp "$SESS_A/pieces/apps/player_app/view.txt" "$PROOF_DIR/sessA_view.txt" 2>/dev/null
cp "$SESS_B/pieces/apps/player_app/view.txt" "$PROOF_DIR/sessB_view.txt" 2>/dev/null

# test-room cleanup (leave the project's real rooms alone)
rm -rf "rooms/$ROOM"
# note: the ledger entry for the test room is left in data/master_ledger.txt
# intentionally (it's an append-only log by design - see PITFALL discussion
# in arch-re&test.txt) but is harmless: chat_replay_ledger.+x will just
# recreate an empty-ish rooms/$ROOM/ if ever replayed, no real room used.

echo "=== proof saved to: $PROOF_DIR ==="
echo
if [ "$FAIL" = "1" ]; then
    echo "=== OVERALL: FAIL - see above ==="
    exit 1
else
    echo "=== OVERALL: PASS ==="
    exit 0
fi
