#!/bin/bash
# run_xod_agent.sh — Orchestrate LLM → FSM → TOM loop for one agent (C ops only)
#
# In default (mock) mode: runs the agent cycle loop against the current
# session directory without a live WSR engine.  The FSM writes keycodes
# to history.txt but they are not processed, so state is static.
#
# In live mode (--live): starts prisc+x running main_loop.pal in the
# background so injected keys are processed and the frame updates between
# cycles.  Each cycle: LLM observes → FSM injects keys → prisc+x ticks
# → TOM reads results → fitness scored → next cycle.
#
# Usage: ./run_xod_agent.sh <session_dir> [goal] [model] [iterations] [--live]
#   session_dir: path to an existing WSR session
#   goal: what the agent is trying to achieve (default: survive)
#   model: Ollama model name (default: gemma3:1b)
#   iterations: how many LLM→FSM cycles to run (default: 10)
#   --live: start a background prisc+x main_loop to process injected keys
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
XOD_DIR="${SCRIPT_DIR}"
SESSION_DIR="${1:-.}"
GOAL="${2:-survive}"
MODEL="${3:-gemma3:1b}"
ITERATIONS="${4:-10}"
LIVE_MODE=0

# Parse trailing --live flag
for arg in "$@"; do
    case "$arg" in
        --live) LIVE_MODE=1 ;;
    esac
done

if [ ! -d "${SESSION_DIR}" ]; then
    echo "Session dir not found: ${SESSION_DIR}"
    exit 1
fi

echo "=== XOD Agent Runner (C ops only) ==="
echo "Session: ${SESSION_DIR}"
echo "Goal:    ${GOAL}"
echo "Iterations: ${ITERATIONS}"
echo "Live WSR: ${LIVE_MODE}"
echo ""

cd "${SESSION_DIR}"

# Verify compiled ops exist
for op in llm_brain fsm_controller tom_layer wsr_fitness; do
    if [ ! -x "${XOD_DIR}/ops/+x/${op}.+x" ]; then
        echo "Missing op: ${XOD_DIR}/ops/+x/${op}.+x"
        exit 1
    fi
done

export PRISC_PROJECT_ROOT="${SESSION_DIR}"
export PRISC_PROJECT_ID="wsr-pal"

WSR_PID=""

cleanup() {
    if [ -n "${WSR_PID}" ] && kill -0 "${WSR_PID}" 2>/dev/null; then
        kill "${WSR_PID}" 2>/dev/null || true
        wait "${WSR_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT

if [ "${LIVE_MODE}" -eq 1 ]; then
    echo "Starting live WSR engine (prisc+x main_loop.pal)..."
    # Clear stale history so old keys don't replay
    > "${SESSION_DIR}/pieces/apps/player_app/history.txt" 2>/dev/null || true
    rm -f "${SESSION_DIR}/pieces/display/layout_changed.txt" 2>/dev/null || true
    # Ensure current_layout.txt has a valid starting layout
    if [ -f "${SESSION_DIR}/pieces/display/current_layout.txt" ]; then
        : # keep existing
    else
        echo "pieces/chtpm/layouts/wsr_main_menu.chtpm" > "${SESSION_DIR}/pieces/display/current_layout.txt"
    fi
    # Ensure state.txt has a starting cursor
    echo "history_cursor=0" > "${SESSION_DIR}/pieces/apps/player_app/state.txt" 2>/dev/null || true
    echo "active_target_id=wsr_main_menu" >> "${SESSION_DIR}/pieces/apps/player_app/state.txt"
fi

for i in $(seq 1 "${ITERATIONS}"); do
    echo "--- Cycle ${i}/${ITERATIONS} ---"

    # Step 1: LLM Brain observes state and decides
    echo "  [LLM] Thinking..."
    decision=$(PRISC_PROJECT_ROOT="${SESSION_DIR}" "${XOD_DIR}/ops/+x/llm_brain.+x" "${SESSION_DIR}" "${GOAL}" 2>/dev/null || echo '{"action":"wait"}')
    action=$(echo "${decision}" | grep -o '"action":"[^"]*"' | cut -d'"' -f4)
    reason=$(echo "${decision}" | grep -o '"reason":"[^"]*"' | cut -d'"' -f4)
    conf=$(echo "${decision}" | grep -o '"confidence":[0-9.]*' | cut -d: -f2)
    echo "  [LLM] Decision: ${action} (conf=${conf}) ${reason}"

    # Step 2: FSM Controller executes the decision
    echo "  [FSM] Executing ${action}..."

    # Clear history.txt before FSM injects keys — main_loop.pal starts
    # read_history at position 0 each fresh process invocation, so stale
    # keys from previous cycles would replay if not cleared
    if [ "${LIVE_MODE}" -eq 1 ]; then
        > "${SESSION_DIR}/pieces/apps/player_app/history.txt" 2>/dev/null || true
    fi

    PRISC_PROJECT_ROOT="${SESSION_DIR}" "${XOD_DIR}/ops/+x/fsm_controller.+x" 2>/dev/null || true

    # In live mode, process injected keys: run main_loop.pal to drain history.txt
    if [ "${LIVE_MODE}" -eq 1 ]; then
        # Run main_loop.pal to process keys (it exits when history is drained)
        timeout 5 "${XOD_DIR}/system/prisc+x" pal/main_loop.pal >"${SESSION_DIR}/state/wsr_loop.log" 2>&1 || true
        # Handle layout changes: if wsr_menu_input wrote a GOTO command to
        # layout_changed.txt, update current_layout.txt so compose_frame
        # renders the correct screen next tick
        if [ -f "${SESSION_DIR}/pieces/display/layout_changed.txt" ]; then
            new_layout=$(grep -v '^\s*$' "${SESSION_DIR}/pieces/display/layout_changed.txt" | tail -1)
            if [ -n "$new_layout" ]; then
                echo "$new_layout" > "${SESSION_DIR}/pieces/display/current_layout.txt"
                grep -v '^'"$(echo "$new_layout" | sed 's/[\/&]/\\&/g')"'$\|^\s*'$'\r''\?$' "${SESSION_DIR}/pieces/display/layout_changed.txt" > /tmp/_lc.tmp 2>/dev/null || true
                mv /tmp/_lc.tmp "${SESSION_DIR}/pieces/display/layout_changed.txt" 2>/dev/null || > "${SESSION_DIR}/pieces/display/layout_changed.txt"
                echo "  [WSR] Layout changed to: $new_layout"
            fi
        fi
        # Recompose the frame with the updated layout
        "${XOD_DIR}/ops/+x/wsr_compose_frame.+x" 2>/dev/null || true
        sleep 0.5
    fi

    # Step 3: TOM Layer updates beliefs about other agents
    echo "  [TOM] Updating beliefs..."
    PRISC_PROJECT_ROOT="${SESSION_DIR}" "${XOD_DIR}/ops/+x/tom_layer.+x" 2>/dev/null || true

    # Step 4: Score the result (real fitness from corp_ORB)
    echo "  [FITNESS] Scoring..."
    PRISC_PROJECT_ROOT="${SESSION_DIR}" "${XOD_DIR}/ops/+x/wsr_fitness.+x" \
        "projects/wsr-pal/pieces/corp_ORB/state.txt" \
        "projects/wsr-pal/pieces/corp_ORB/price_history.txt" 2>/dev/null || true

    if [ -f "pieces/display/fitness.txt" ]; then
        fitness=$(grep "^fitness=" pieces/display/fitness.txt | cut -d= -f2)
        echo "  [FITNESS] Score: ${fitness}"
    fi

    if [ "${LIVE_MODE}" -eq 0 ]; then
        sleep 1
    fi
done

echo ""
echo "=== Agent Run Complete ==="
echo "Event bus:"
tail -20 "${SESSION_DIR}/pieces/apps/player_app/interact_relay.txt" 2>/dev/null || true

# Generate HTML chart from event log
echo ""
echo "=== Generating HTML Chart ==="
PRISC_PROJECT_ROOT="${SESSION_DIR}" "${XOD_DIR}/ops/+x/wsr_chart.+x" \
    --events "${SESSION_DIR}/pieces/apps/player_app/interact_relay.txt" \
    "${XOD_DIR}/xod_chart.html" 2>/dev/null || true
echo "Chart: ${XOD_DIR}/xod_chart.html"
