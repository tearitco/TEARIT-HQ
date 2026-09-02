#!/bin/bash
# pvp_duel.sh - TSC_ELO PvP-over-P2P scenario (D2/D3), driven by
# fsm-driver.sh. Two real subharnesses (each = one full TSC_ELO host
# session + its own Match Setup WIDGIT + its own palnet_peer node) prove
# a real duel over the house P2P stack, asserted on REAL network
# artifacts only (PITFALL 21):
#
#   P1: presence   - net/presence/ has 2 live tsc_duel nodes
#   P2: challenge  - B's net/inbox.txt carries A's CHALLENGE (wire)
#   P3: accept     - A's net/inbox.txt carries B's ACCEPT (wire)
#   P4: moves      - both inboxes carry the other side's MOVE (wire)
#   P5: convergence- per-session ledgers agree on the ordered action seq
#
# Every user action goes through the widget's REAL key path: the harness
# injects a key into the widget session's keyboard/history.txt
# (KEY_PRESSED: <dec>), the widget's chtpm_parser_pal processes it as a
# real keystroke (digit nav-jump + Enter execute -> send_command KEY:n
# -> interact_relay.txt -> setup_menu_input -> host cmd-bus -> tsc_setup
# -> net/outbox.txt -> peer -> remote inbox). Nothing writes the host
# cmd-bus or net files directly during those states.
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
OPS="$HARNESS_DIR/ops/+x"
source "$HARNESS_DIR/fsm-driver.sh"

PROOF_DIR="$PROJECT_DIR/proof/pvp-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0

cleanup() {
    echo; echo "--- cleanup ---"
    bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
}
trap cleanup EXIT INT TERM

# ---- wrappers (one line each; all real logic in the ops) ----
key()   { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.3; }
focus() { "$OPS/tk_focus_item.+x" "$1" "$2" "$3" >/dev/null; sleep 0.3; }
check() { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }
cmd()   { "$OPS/tsc_cmd.+x" "$1" "$2" "${3:-}"; sleep 0.4; }

# ---- pluggable answer source (TSC_P2P_PVP.md D2) ----
ANSWER_MODE="${TSC_ANSWER_MODE:-auto}"
ANSWER="$OPS/tsc_answer.+x"
# Ask the answer source for a move, then play it through the WIDGET's
# REAL input chain: focus its "Play: <MOVE>" METHOD row + Enter. That is
# a genuine widget UI move (parser digit-jump -> send_command KEY:n ->
# interact_relay.txt -> setup_menu_input -> host cmd-bus -> tsc_setup),
# the exact path the CHALLENGE/ACCEPT states already proved.
move_key() { # <widget_sess> <widget_frame> <context_prompt>
    local w="$1" wf="$2" prompt="$3" mv
    mv="$("$ANSWER" "$ANSWER_MODE" "$prompt" "$HARNESS_DIR/book.txt")"
    case "$mv" in
        heavy) focus "$w" "$wf" "Play: HEAVY" ;;
        heal)  focus "$w" "$wf" "Play: HEAL" ;;
        block) focus "$w" "$wf" "Play: BLOCK" ;;
        *)     mv="strike"; focus "$w" "$wf" "Play: STRIKE" ;;
    esac
    key "$w" 13
    echo "$mv"
}

reset_shared_state() {
    # Seed the SHARED config once from the clean PvP seed (design D1:
    # config is shared/symlinked; the per-session files carry the proof).
    if [ -f "$PROJECT_DIR/pieces/system/config.seed.txt" ]; then
        cp "$PROJECT_DIR/pieces/system/config.seed.txt" \
           "$PROJECT_DIR/pieces/system/config.txt"
    fi
    rm -rf "$PROJECT_DIR/net/presence"
    mkdir -p "$PROJECT_DIR/net/presence"
}

# ---- state handlers (each returns next-state name or "" to retry) ----
h_boot() {
    if [ -z "${SESS_A:-}" ]; then
        bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
        reset_shared_state
        sleep 1
        cd "$PROJECT_DIR"
        # nice keeps the two subharnesses from starving the machine; the
        # harness's own pacing (sleeps below) dominates the wall time.
        NO_GL=1 setsid nice -n 10 bash button.sh run > /tmp/tsc_duel_a.log 2>&1 < /dev/null & disown
        sleep 1
        NO_GL=1 setsid nice -n 10 bash button.sh run > /tmp/tsc_duel_b.log 2>&1 < /dev/null & disown
        sleep 4
    fi
    [ -d "$PROJECT_DIR/pieces/sessions" ] || return 1
    SESS_A="$(ls -dt "$PROJECT_DIR"/pieces/sessions/*/ | sed -n '2p')"; SESS_A="${SESS_A%/}"
    SESS_B="$(ls -dt "$PROJECT_DIR"/pieces/sessions/*/ | sed -n '1p')"; SESS_B="${SESS_B%/}"
    [ -n "$SESS_A" ] && [ -n "$SESS_B" ] || return 1
    # Persist as soon as found so a retry never re-launches the hosts.
    fsm_set SESS_A "$SESS_A"; fsm_set SESS_B "$SESS_B"
    # Map each widget session to its host via its own focus.txt.
    local w
    WIDGET_A=""; WIDGET_B=""
    for w in $(ls -dt "$PROJECT_DIR"/widgets/setup/pieces/sessions/*/ 2>/dev/null); do
        w="${w%/}"
        local root
        root="$(grep -s '^session_root=' "$w/pieces/system/focus.txt" 2>/dev/null | cut -d= -f2-)"
        [ -n "$root" ] || continue
        if [ "$root" = "$SESS_A" ]; then WIDGET_A="$w"; fi
        if [ "$root" = "$SESS_B" ]; then WIDGET_B="$w"; fi
    done
    [ -n "$WIDGET_A" ] && [ -n "$WIDGET_B" ] || return 1
    FRAME_A="$SESS_A/pieces/display/current_frame.txt"
    FRAME_B="$SESS_B/pieces/display/current_frame.txt"
    WFRAME_A="$WIDGET_A/pieces/display/current_frame.txt"
    WFRAME_B="$WIDGET_B/pieces/display/current_frame.txt"
    CFG="$PROJECT_DIR/pieces/system/config.txt"
    GID="$(grep -s '^game_id=' "$CFG" | cut -d= -f2-)"
    LED_A="$SESS_A/pieces/system/games/$GID/ledger.txt"
    LED_B="$SESS_B/pieces/system/games/$GID/ledger.txt"
    # PITFALL 54: never render/assert before a non-empty current_frame.
    [ -s "$FRAME_A" ] && [ -s "$FRAME_B" ] || return 1
    fsm_set WIDGET_A "$WIDGET_A"; fsm_set WIDGET_B "$WIDGET_B"
    fsm_set FRAME_A "$FRAME_A"; fsm_set FRAME_B "$FRAME_B"
    fsm_set WFRAME_A "$WFRAME_A"; fsm_set WFRAME_B "$WFRAME_B"
    fsm_set CFG "$CFG"; fsm_set GID "$GID"
    fsm_set LED_A "$LED_A"; fsm_set LED_B "$LED_B"
    echo "host A: ${SESS_A##*/}  (widget ${WIDGET_A##*/})"
    echo "host B: ${SESS_B##*/}  (widget ${WIDGET_B##*/})"
    echo "game_id: $GID"
    echo "PRESENCE"
}

h_presence() {
    local n
    n="$(grep -l '^kind=tsc_duel$' "$PROJECT_DIR"/net/presence/*.txt 2>/dev/null | wc -l)"
    [ "$n" -ge 2 ] || return 1
    echo "P1 (presence): $n live tsc_duel nodes in net/presence/"
    # Distinct player names in each host's own pending (per-session).
    cmd "$SESS_A" PLAYER "P1_$$"
    cmd "$SESS_B" PLAYER "P2_$$"
    echo "CHALLENGE"
}

h_challenge() {
    # REAL_KEYS: focus + Enter CHALLENGE on WIDGET A.
    focus "$WIDGET_A" "$WFRAME_A" "PvP: CHALLENGE"
    key "$WIDGET_A" 13
    sleep 1
    # Real wire artifact: B's inbox must carry A's CHALLENGE: message.
    grep -sq "CHALLENGE:P1_$$" "$SESS_B/net/inbox.txt" 2>/dev/null || return 1
    echo "P2 (challenge over wire): B inbox has A's CHALLENGE:"
    grep -s "CHALLENGE" "$SESS_B/net/inbox.txt" | tail -1 | sed 's/^/  /'
    echo "ACCEPT"
}

h_accept() {
    # REAL_KEYS: focus + Enter ACCEPT on WIDGET B.
    focus "$WIDGET_B" "$WFRAME_B" "PvP: ACCEPT"
    key "$WIDGET_B" 13
    sleep 1
    # Playing state lands in the SHARED config only after BOTH the wire
    # ACCEPT and B's local PVP:ACCEPT ran.
    local st mo
    st="$(grep -s '^game_state=' "$CFG" | cut -d= -f2-)"
    mo="$(grep -s '^mode=' "$CFG" | cut -d= -f2-)"
    [ "$st" = "playing" ] && [ "$mo" = "PvP" ] || return 1
    # Real wire artifact: A's inbox has B's ACCEPT.
    grep -sq "|ACCEPT$" "$SESS_A/net/inbox.txt" 2>/dev/null || return 1
    echo "P3 (accept over wire): A inbox has B's ACCEPT; state=$st mode=$mo"
    echo "A_MOVE"
}

h_a_move() {
    # A is player_1; on turn 0 cp=1 so A may act. Answer source chooses
    # the move; it is injected as a REAL key on WIDGET A.
    local mv
    mv="$(move_key "$WIDGET_A" "$WFRAME_A" "We open the duel. Attack to probe.")"
    local t
    t="$(grep -s '^current_turn=' "$CFG" | cut -d= -f2-)"
    [ "$t" = "1" ] || return 1
    # Real wire artifact: B's inbox has A's MOVE:mv
    grep -sq "|MOVE:$mv$" "$SESS_B/net/inbox.txt" 2>/dev/null || return 1
    fsm_set A_MOVE "$mv"
    echo "P4a (A move '$mv' over wire): B inbox has MOVE; turn=$t"
    echo "B_VERIFY"
}

h_b_verify() {
    # B's PER-SESSION ledger must already contain A's move (via=net).
    if [ ! -s "$LED_B" ] || ! grep -sq "MOVE:$A_MOVE" "$LED_B" 2>/dev/null; then
        { echo "  [debug] LED_B=$LED_B A_MOVE=$A_MOVE"
          [ -f "$LED_B" ] && sed 's/^/  ledger: /' "$LED_B" || echo "  ledger: MISSING"
          echo "  [debug] B inbox:"; sed 's/^/  in: /' "$SESS_B/net/inbox.txt" 2>/dev/null
        } >&2
        return 1
    fi
    echo "P4b: B's per-session ledger has A's move '$A_MOVE' (via=net):"
    grep -s "MOVE:$A_MOVE" "$LED_B" | tail -1 | sed 's/^/  /'
    echo "B_MOVE"
}

h_b_move() {
    # B is player_2; after A's move turn=1, cp=2 so B may act.
    local mv
    mv="$(move_key "$WIDGET_B" "$WFRAME_B" "They struck us. Hit back.")"
    local t
    t="$(grep -s '^current_turn=' "$CFG" | cut -d= -f2-)"
    [ "$t" = "2" ] || return 1
    grep -sq "|MOVE:$mv$" "$SESS_A/net/inbox.txt" 2>/dev/null || return 1
    fsm_set B_MOVE "$mv"
    echo "P4c (B move '$mv' over wire): A inbox has MOVE; turn=$t"
    echo "A_VERIFY"
}

h_a_verify() {
    # A's PER-SESSION ledger must contain B's move (via=net).
    [ -s "$LED_A" ] || return 1
    grep -sq "MOVE:$B_MOVE" "$LED_A" 2>/dev/null || return 1
    echo "P4d: A's per-session ledger has B's move '$B_MOVE' (via=net):"
    grep -s "MOVE:$B_MOVE" "$LED_A" | tail -1 | sed 's/^/  /'
    echo "CONVERGENCE"
}

h_convergence() {
    # P5: the two per-session ledgers are the SAME ordered action
    # sequence (seq|user|action), differing only in sender/via.
    [ -s "$LED_A" ] && [ -s "$LED_B" ] || return 1
    local seq_a seq_b
    seq_a="$(cut -d'|' -f1,3,4 "$LED_A")"
    seq_b="$(cut -d'|' -f1,3,4 "$LED_B")"
    [ "$seq_a" = "$seq_b" ] || return 1
    echo "P5 (convergence): both ledgers agree, $(wc -l < "$LED_A") events:"
    cut -d'|' -f4 "$LED_A" | sed 's/^/  /'
    # Preserve the proof before cleanup deletes the throwaway sessions.
    # Each artifact keeps its side (A/B) in the name - a flattened copy
    # would let B's ledger overwrite A's and lose the P5 evidence.
    mkdir -p "$PROOF_DIR"
    cp -r "$PROJECT_DIR/net/presence" "$PROOF_DIR/presence"
    cp "$SESS_A/net/inbox.txt"  "$PROOF_DIR/inbox_A.txt"
    cp "$SESS_B/net/inbox.txt"  "$PROOF_DIR/inbox_B.txt"
    cp "$LED_A"                 "$PROOF_DIR/ledger_A.txt"
    cp "$LED_B"                 "$PROOF_DIR/ledger_B.txt"
    cp "$CFG"                   "$PROOF_DIR/config.txt"
    cp "$FRAME_A"               "$PROOF_DIR/frame_A.txt"
    cp "$FRAME_B"               "$PROOF_DIR/frame_B.txt"
    cp "$WFRAME_A"              "$PROOF_DIR/wframe_A.txt"
    cp "$WFRAME_B"              "$PROOF_DIR/wframe_B.txt"
    cp "$SESS_A/net/state.txt"  "$PROOF_DIR/state_A.txt" 2>/dev/null || true
    cp "$SESS_B/net/state.txt"  "$PROOF_DIR/state_B.txt" 2>/dev/null || true
    printf "A=%s\nB=%s\nWIDGET_A=%s\nWIDGET_B=%s\nGID=%s\n" \
        "$SESS_A" "$SESS_B" "$WIDGET_A" "$WIDGET_B" "$GID" \
        > "$PROOF_DIR/session.info"
    # Stable pointer to the newest archive.
    ln -sfn "$PROOF_DIR" "$PROJECT_DIR/proof/latest"
    echo "DONE"
}

# ---- FSM: register states (state, timeout_s, handler) ----
fsm_register BOOT         45 h_boot
fsm_register PRESENCE     30 h_presence
fsm_register CHALLENGE    30 h_challenge
fsm_register ACCEPT       30 h_accept
fsm_register A_MOVE       30 h_a_move
fsm_register B_VERIFY     20 h_b_verify
fsm_register B_MOVE       30 h_b_move
fsm_register A_VERIFY     20 h_a_verify
fsm_register CONVERGENCE  15 h_convergence

echo "=== TSC_ELO PvP duel over the house P2P mesh ==="
echo "answer mode: $ANSWER_MODE"
if ! fsm_run; then
    echo "=== FSM FAILED in state $FSM_STATE ==="
    echo "FAIL"
    exit 1
fi
echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1; fi
echo "=== OVERALL: PASS ==="
exit 0
