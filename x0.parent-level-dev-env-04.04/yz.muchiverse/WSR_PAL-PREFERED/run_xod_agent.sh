#!/bin/bash
# run_xod_agent.sh — Orchestrate LLM → FSM → TOM loop for one agent (C ops only)
#
# Usage: ./run_xod_agent.sh <session_dir> [goal] [model] [iterations]
#   session_dir: path to an existing WSR session
#   goal: what the agent is trying to achieve (default: survive)
#   model: Ollama model name (default: gemma3:1b)
#   iterations: how many LLM→FSM cycles to run (default: 10)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
XOD_DIR="${SCRIPT_DIR}"
SESSION_DIR="${1:-.}"
GOAL="${2:-survive}"
MODEL="${3:-gemma3:1b}"
ITERATIONS="${4:-10}"

if [ ! -d "${SESSION_DIR}" ]; then
    echo "Session dir not found: ${SESSION_DIR}"
    exit 1
fi

echo "=== XOD Agent Runner (C ops only) ==="
echo "Session: ${SESSION_DIR}"
echo "Goal:    ${GOAL}"
echo "Iterations: ${ITERATIONS}"
echo ""

cd "${SESSION_DIR}"

# Verify compiled ops exist
for op in llm_brain fsm_controller tom_layer wsr_fitness; do
    if [ ! -x "${XOD_DIR}/ops/+x/${op}.+x" ]; then
        echo "Missing op: ${XOD_DIR}/ops/+x/${op}.+x"
        exit 1
    fi
done

for i in $(seq 1 "${ITERATIONS}"); do
    echo "--- Cycle ${i}/${ITERATIONS} ---"

    # Step 1: LLM Brain observes state and decides
    echo "  [LLM] Thinking..."
    decision=$(PRISC_PROJECT_ROOT="${XOD_DIR}" "${XOD_DIR}/ops/+x/llm_brain.+x" "${SESSION_DIR}" "${GOAL}" 2>/dev/null || echo '{"action":"wait"}')
    action=$(echo "${decision}" | grep -o '"action":"[^"]*"' | cut -d'"' -f4)
    reason=$(echo "${decision}" | grep -o '"reason":"[^"]*"' | cut -d'"' -f4)
    conf=$(echo "${decision}" | grep -o '"confidence":[0-9.]*' | cut -d: -f2)
    echo "  [LLM] Decision: ${action} (conf=${conf}) ${reason}"

    # Step 2: FSM Controller executes the decision
    echo "  [FSM] Executing ${action}..."
    PRISC_PROJECT_ROOT="${XOD_DIR}" "${XOD_DIR}/ops/+x/fsm_controller.+x" 2>/dev/null || true

    # Step 3: TOM Layer updates beliefs about other agents
    echo "  [TOM] Updating beliefs..."
    PRISC_PROJECT_ROOT="${XOD_DIR}" "${XOD_DIR}/ops/+x/tom_layer.+x" 2>/dev/null || true

    # Step 4: Score the result (real fitness from corp_ORB)
    echo "  [FITNESS] Scoring..."
    PRISC_PROJECT_ROOT="${XOD_DIR}" "${XOD_DIR}/ops/+x/wsr_fitness.+x" \
        "projects/wsr-pal/pieces/corp_ORB/state.txt" \
        "projects/wsr-pal/pieces/corp_ORB/price_history.txt" 2>/dev/null || true

    if [ -f "pieces/display/fitness.txt" ]; then
        fitness=$(grep "^fitness=" pieces/display/fitness.txt | cut -d= -f2)
        echo "  [FITNESS] Score: ${fitness}"
    fi

    sleep 1
done

echo ""
echo "=== Agent Run Complete ==="
echo "Event bus:"
tail -20 "${XOD_DIR}/pieces/apps/player_app/interact_relay.txt" 2>/dev/null || true