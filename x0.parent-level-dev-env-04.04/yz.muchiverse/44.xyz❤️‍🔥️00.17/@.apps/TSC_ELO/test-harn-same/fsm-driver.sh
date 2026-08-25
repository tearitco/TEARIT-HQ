#!/bin/bash
# fsm-driver.sh - the FSM engine for the TSC_ELO PvP harness
# (TSC_P2P_PVP.md, D2). Sourced by scenarios/pvp_duel.sh.
#
# A scenario registers its states via fsm_state NAME TIMEOUT_S ENTER_HANDLER
# and advances with `fsm_advance`. This driver:
#   * tracks the current state + an optional payload string
#   * runs each state's ENTER_HANDLER, which may return:
#        0/""   -> still in this state, driver retries after FSM_TICK
#        NAME   -> transition to state NAME (next cycle)
#   * hard-kills the state after TIMEOUT_S with a FSM_TIMEOUT failure
#   * counts transitions so a scenario can cap total duel length
# The states themselves hold NO timing logic of their own (that is the
# driver's job), and the transition table lives in the scenario, not
# here - this file is scenario-agnostic.
set -u

FSM_STATE="${FSM_STATE:-BOOT}"
FSM_PAYLOAD="${FSM_PAYLOAD:-}"
FSM_TICK_S="${FSM_TICK_S:-0.5}"
FSM_TRANSITIONS=0
FSM_MAX_TRANSITIONS="${FSM_MAX_TRANSITIONS:-120}"
# CPU/wallclock protections (the FSM must never spin or hang the machine):
#   * every handler invocation is bounded by coreutils `timeout`
#   * the whole run is bounded by an overall wallclock cap
#   * between retries the driver sleeps FSM_TICK_S (never busy-polls)
FSM_WALL_S="${FSM_WALL_S:-600}"
FSM_START_S="$(date +%s)"
FSM_FAIL=0
# Handlers run in a $(...) subshell, so globals they assign are lost.
# Anything a later state needs must go through this shared state file.
FSM_STATE_FILE="${FSM_STATE_FILE:-$HARNESS_DIR/.fsm-state}"
rm -f "$FSM_STATE_FILE"

fsm_set() { # <var> <value> - persist a var into the shared state file
    local f="$FSM_STATE_FILE"
    mkdir -p "$(dirname "$f")" 2>/dev/null
    if [ -f "$f" ] && grep -qs "^$1=" "$f" 2>/dev/null; then
        sed -i "s|^$1=.*|$1=$2|" "$f"
    else
        echo "$1=$2" >> "$f"
    fi
}

fsm_load_state() { [ -f "$FSM_STATE_FILE" ] && . "$FSM_STATE_FILE"; }


# associative-array-ish env file backing, so states/handlers can be
# defined in the scenario after sourcing this file
FSM_STATE_TIMEOUT=""
FSM_STATE_HANDLER=""

fsm_register() { # <state> <timeout_s> <handler_fn>
    FSM_STATE_TIMEOUT="${FSM_STATE_TIMEOUT}|$1=$2"
    FSM_STATE_HANDLER="${FSM_STATE_HANDLER}|$1=$3"
}

fsm_timeout_for() { # <state> -> echoes seconds (default 30)
    local kv to=30
    for kv in ${FSM_STATE_TIMEOUT//|/ }; do
        if [ "${kv%%=*}" = "$1" ]; then to="${kv#*=}"; break; fi
    done
    echo "$to"
}

fsm_handler_for() { # <state> -> echoes handler name
    local kv h=":"
    for kv in ${FSM_STATE_HANDLER//|/ }; do
        if [ "${kv%%=*}" = "$1" ]; then h="${kv#*=}"; break; fi
    done
    echo "$h"
}

# Enter the current state: runs its handler up to its deadline.
fsm_step() {
    local state="$FSM_STATE"
    local timeout_s handler
    timeout_s="$(fsm_timeout_for "$state")"
    handler="$(fsm_handler_for "$state")"
    if [ -z "$handler" ] || [ "$handler" = ":" ]; then
        echo "FSM ERROR: no handler registered for state '$state'"
        return 1
    fi
    local deadline
    deadline=$(( $(date +%s) + timeout_s ))
    local result=""
    while [ "$(date +%s)" -lt "$deadline" ]; do
        fsm_load_state
        result="$($handler)"
        # Handlers log freely, but the LAST line of their output is the
        # transition target (empty or "FSM_STAY" means stay in state).
        # The rest of the handler's output is the human-readable proof
        # (P1..P5 lines) - print it (indented) so the run log shows it.
        local target nlines
        target="$(printf '%s\n' "$result" | tail -1)"
        nlines="$(printf '%s\n' "$result" | grep -c . 2>/dev/null)"
        if [ "${nlines:-0}" -gt 1 ]; then
            printf '%s\n' "$result" | sed '$d' | sed 's/^/  /'
        fi
        if [ "$target" = "FSM_TIMEOUT" ]; then
            echo "FSM TIMEOUT in state '$state'"
            return 1
        fi
        if [ -n "$target" ] && [ "$target" != "FSM_STAY" ]; then
            FSM_TRANSITIONS=$((FSM_TRANSITIONS + 1))
            if [ "$FSM_TRANSITIONS" -gt "$FSM_MAX_TRANSITIONS" ]; then
                echo "FSM OVERFLOW: too many transitions"
                return 1
            fi
            echo "FSM: $state -> $target"
            FSM_STATE="$target"
            return 0
        fi
        sleep "$FSM_TICK_S"
    done
    echo "FSM TIMEOUT waiting for '$state' handler to advance"
    return 1
}

fsm_run() { # run until FSM_STATE == DONE or a failure
    while [ "$FSM_STATE" != "DONE" ]; do
        if [ $(( $(date +%s) - FSM_START_S )) -gt "$FSM_WALL_S" ]; then
            echo "FSM OVERFLOW: overall wallclock cap ${FSM_WALL_S}s exceeded"
            FSM_FAIL=1
            return 1
        fi
        if ! fsm_step; then
            FSM_FAIL=1
            return 1
        fi
    done
    return 0
}
