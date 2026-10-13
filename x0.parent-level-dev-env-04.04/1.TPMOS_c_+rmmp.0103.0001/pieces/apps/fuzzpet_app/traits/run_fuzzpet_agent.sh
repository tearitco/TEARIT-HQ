#!/bin/bash
# run_fuzzpet_agent.sh — Automation loop for self-directed FuzzPet
# Usage: ./run_fuzzpet_agent.sh [iterations] [model] [goal]
#   iterations: loop count (0 = infinite, default: 10)
#   model:      Ollama model (default: gemma3:1b)
#   goal:       survive | explore | evolve (default: survive)
#
# Pipeline:
#   1. fuzzpet_brain reads state.txt + history
#      → calls Ollama API
#      → writes PET_ACTION to relay + syncs behavior.pdl
#   2. fuzzpet_fsm reads last PET_ACTION from relay
#      → updates state.txt directly
#      → writes ACTION_EXECUTED to relay
#   3. history.txt updated automatically by brain
#
# Set PRISC_PROJECT_ROOT to the 1.TPMOS project directory.

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${PRISC_PROJECT_ROOT:-$SCRIPT_DIR/../../..}"

ITERATIONS="${1:-10}"
MODEL="${2:-gemma3:1b}"
GOAL="${3:-survive}"

BRAIN="$PROJECT_ROOT/pieces/apps/fuzzpet_app/traits/+x/fuzzpet_brain.+x"
FSM="$PROJECT_ROOT/pieces/apps/fuzzpet_app/traits/+x/fuzzpet_fsm.+x"

if [ ! -x "$BRAIN" ]; then
    echo "WARNING: fuzzpet_brain.+x not found, compiling..."
    gcc -O2 -w "$PROJECT_ROOT/pieces/apps/fuzzpet_app/traits/fuzzpet_brain.c" -o "$BRAIN" -lm 2>&1
fi

if [ ! -x "$FSM" ]; then
    echo "WARNING: fuzzpet_fsm.+x not found, compiling..."
    gcc -O2 -w "$PROJECT_ROOT/pieces/apps/fuzzpet_app/traits/fuzzpet_fsm.c" -o "$FSM" -lm 2>&1
fi

mkdir -p "$PROJECT_ROOT/pieces/apps/player_app"
RELAY="$PROJECT_ROOT/pieces/apps/player_app/interact_relay.txt"
touch "$RELAY"

echo "=== FuzzPet AI Agent Loop ==="
echo "Project: $PROJECT_ROOT"
echo "Model:   $MODEL"
echo "Goal:    $GOAL"
echo "Relay:   $RELAY"
echo "Iterations: $([ "$ITERATIONS" = "0" ] && echo "infinite" || echo "$ITERATIONS")"
echo ""

i=0
while true; do
    i=$((i + 1))
    echo "--- Iteration $i ---"

    # 1. Brain: LLM observation + decision
    echo "[brain] Calling Ollama ($MODEL) for decision..."
    "$BRAIN" "." "$GOAL" "$MODEL" 2>&1
    BRAIN_RC=$?

    if [ $BRAIN_RC -ne 0 ]; then
        echo "[brain] Failed (rc=$BRAIN_RC), trying fallback action directly..."
        echo "PET_ACTION|feed|0.5|brain_failed_fallback|pet=Fuzzball" >> "$RELAY"
    fi

    # Read the action that was written
    LAST_ACTION=$(grep "^PET_ACTION|" "$RELAY" | tail -1)
    echo "[fsm] Dispatching: $LAST_ACTION"

    # 2. FSM: Execute the action
    "$FSM" "fuzzball" 2>&1
    FSM_RC=$?

    # 3. Show current state
    STATE_FILE="$PROJECT_ROOT/pieces/apps/fuzzpet_app/fuzzpet/state.txt"
    if [ -f "$STATE_FILE" ]; then
        echo "[state] $(grep -E '^(hunger|happiness|energy|level)=' "$STATE_FILE" | tr '\n' ' ')"
    fi
    echo ""

    # Check loop condition
    if [ "$ITERATIONS" != "0" ] && [ $i -ge $ITERATIONS ]; then
        echo "=== Completed $i iterations ==="
        break
    fi

    sleep 1
done

echo "FuzzPet agent loop ended."
