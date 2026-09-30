#!/bin/bash
# run.sh - the vvarware brain loop. Picks a goal, asks llama3.2:3b which
# tool to use, dispatches to the feature op, loops. Safe defaults keep it
# alive even with no network.
#
# Usage: run.sh [--one]        --one = exactly one goal cycle, then exit
#
# KISS v1: deterministic guardrails first, LLM second. If the model is
# unreachable, fall back to a simple scripted activity so the monad
# still "lives" (moves, charges, chats the ledger) instead of dying.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/oplib.sh"

ONE_SHOT=0
[ "$1" = "--one" ] && ONE_SHOT=1

dispatch() {
    local tool="$1"
    shift
    case "$tool" in
        battle)            bash "$SCRIPT_DIR/../features/battle.sh" "$@" ;;
        move)              bash "$SCRIPT_DIR/../features/move.sh" "$@" ;;
        learn)             bash "$SCRIPT_DIR/../features/learn.sh" "$@" ;;
        build_self)        bash "$SCRIPT_DIR/../features/build_self.sh" "$@" ;;
        charge)            bash "$SCRIPT_DIR/../features/charge.sh" "$@" ;;
        buy_batteries)     bash "$SCRIPT_DIR/../features/buy_batteries.sh" "$@" ;;
        chat)              bash "$SCRIPT_DIR/../features/chat.sh" "$@" ;;
        write_other_bots)  bash "$SCRIPT_DIR/../features/write_other_bots.sh" "$@" ;;
        *)                 ledger_append "Brain" "unknown tool: $tool" "run.sh" ;;
    esac
}

# fallback_activity - scripted living when the LLM is down
fallback_activity() {
    local battery
    battery="$(read_state battery)"
    if [ "$battery" -lt 30 ]; then
        bash "$SCRIPT_DIR/../features/charge.sh" 10
    elif [ $((RANDOM % 3)) -eq 0 ]; then
        bash "$SCRIPT_DIR/../features/move.sh" $((RANDOM % 3 - 1)) $((RANDOM % 3 - 1))
    else
        bash "$SCRIPT_DIR/../features/learn.sh" "ran fallback activity at $(date '+%H:%M:%S')"
    fi
}

cycle() {
    local goal tool
    goal="$(next_goal)"
    if [ -z "$goal" ]; then
        # self-sustain: generate a modest goal from state
        local battery gold
        battery="$(read_state battery)"
        gold="$(read_state gold)"
        if [ "$battery" -lt 40 ] && [ "$gold" -ge 10 ]; then
            goal="buy batteries and charge up"
        elif [ "$battery" -lt 40 ]; then
            goal="charge my battery"
        elif [ "$gold" -gt 30 ]; then
            goal="buy some batteries"
        else
            goal="explore the house and learn something new"
        fi
    else
        pop_goal
    fi

    ledger_append "Brain" "working on goal: $goal" "run.sh"

    if ! brain_call "$goal"; then
        fallback_activity
        return 0
    fi

    tool="$(tool_name)"
    if [ -z "$tool" ]; then
        ledger_append "Brain" "no tool call in reply (model chose to talk)" "run.sh"
        # treat the reply text as a chat line so the ledger always grows
        local text
        text="$(python3 -c "import json; print(json.load(open('$BRAIN_DIR/llm_reply.json')).get('message',{}).get('content',''))" 2>/dev/null)"
        [ -n "$text" ] && ledger_append "Chat" "brain said: $text" "run.sh"
        return 0
    fi

    local args
    args="$(tool_args)"
    ledger_append "Brain" "dispatching tool $tool ($args)" "run.sh"

    # parse "k=v k2=v2" into positional args
    set -- $args
    dispatch "$tool" "$@"
}

ledger_append "Brain" "vvarware brain online (model $(read_state brain_model))" "run.sh"

while :; do
    cycle
    [ "$ONE_SHOT" -eq 1 ] && break
    sleep 5
done
