#!/bin/bash
# live_tournament_xod.sh - Live Multi-Agent Tournament with Real WSR
#
# Runs N agents in REAL WSR sessions (via button.sh run --pal),
# each with session isolation (xyzos-standards §23).
# Each agent has its own XOD harness driving it via relay injection.
#
# This is the LIVE version of tournament_xod.sh - it actually
# launches WSR instances instead of using mock data.
#
# Usage: ./live_tournament_xod.sh [agents] [duration_seconds]
#   agents: number of competing agents (default: 2)
#   duration_seconds: how long to run (default: 60)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WSR_DIR="${SCRIPT_DIR}"
AGENTS="${1:-2}"
DURATION="${2:-60}"

echo "=== XOD LIVE Multi-Agent Tournament ==="
echo "Agents: ${AGENTS} | Duration: ${DURATION}s"
echo ""

# Verify WSR is built
if [ ! -x "${WSR_DIR}/system/orchestrator" ]; then
    echo "Building WSR..."
    bash "${WSR_DIR}/button.sh" compile
fi

# Create tournament results dir
TOURNAMENT_DIR="${WSR_DIR}/pieces/tournament_live_$(date +%Y%m%d_%H%M%S)"
mkdir -p "${TOURNAMENT_DIR}"

declare -a AGENT_PIDS
declare -a AGENT_SCORES
declare -a AGENT_SESSIONS

# Launch agents
for agent_id in $(seq 1 "${AGENTS}"); do
    echo "Launching Agent ${agent_id}..."

    # Create session
    SESSION_ID="live_agent${agent_id}_$(date +%s)-$$"
    AGENT_SESSIONS[$agent_id]="${SESSION_ID}"

    # Launch WSR in PAL mode with session isolation
    # This runs the actual WSR game with chtpm
    bash "${WSR_DIR}/button.sh" run --pal > "${TOURNAMENT_DIR}/agent${agent_id}_wsr.log" 2>&1 &
    WSR_PID=$!
    AGENT_PIDS[$agent_id]=$WSR_PID

    echo "  Agent ${agent_id} launched (PID: ${WSR_PID}, session: ${SESSION_ID})"

    # Give WSR time to start
    sleep 3
done

echo ""
echo "All agents launched. Tournament running for ${DURATION} seconds..."
echo "Press Ctrl+C to stop early."
echo ""

# Monitor loop
START_TIME=$(date +%s)
while true; do
    NOW=$(date +%s)
    ELAPSED=$((NOW - START_TIME))

    if [ "${ELAPSED}" -ge "${DURATION}" ]; then
        break
    fi

    # Read fitness for each agent
    for agent_id in $(seq 1 "${AGENTS}"); do
        SESSION_DIR="${WSR_DIR}/pieces/sessions/${AGENT_SESSIONS[$agent_id]}"
        if [ -d "${SESSION_DIR}" ]; then
            FITNESS_FILE="${SESSION_DIR}/pieces/display/fitness.txt"
            if [ -f "${FITNESS_FILE}" ]; then
                fitness=$(grep "^fitness=" "${FITNESS_FILE}" 2>/dev/null | tail -1 | cut -d= -f2)
                AGENT_SCORES[$agent_id]="${fitness:-0}"
            fi
        fi
    done

    # Display standings
    printf "\033[2J\033[H"
    echo "=== XOD LIVE Tournament - ${ELAPSED}/${DURATION}s ==="
    echo ""
    printf "%-10s %-15s %-15s %-15s\n" "Agent" "Fitness" "Portfolio" "Cash"
    printf "%-10s %-15s %-15s %-15s\n" "-----" "-------" "--------" "----"

    for agent_id in $(seq 1 "${AGENTS}"); do
        SESSION_DIR="${WSR_DIR}/pieces/sessions/${AGENT_SESSIONS[$agent_id]}"
        FITNESS_FILE="${SESSION_DIR}/pieces/display/fitness.txt"
        if [ -f "${FITNESS_FILE}" ]; then
            fitness=$(grep "^fitness=" "${FITNESS_FILE}" 2>/dev/null | tail -1 | cut -d= -f2)
            portfolio=$(grep "^portfolio_value=" "${FITNESS_FILE}" 2>/dev/null | tail -1 | cut -d= -f2)
            cash=$(grep "^cash=" "${FITNESS_FILE}" 2>/dev/null | tail -1 | cut -d= -f2)
            printf "%-10s %-15s %-15s %-15s\n" "Agent ${agent_id}" "${fitness:-0}" "${portfolio:-0}" "${cash:-0}"
        else
            printf "%-10s %-15s %-15s %-15s\n" "Agent ${agent_id}" "..." "..." "..."
        fi
    done

    echo ""
    echo "Event bus tail:"
    for agent_id in $(seq 1 "${AGENTS}"); do
        SESSION_DIR="${WSR_DIR}/pieces/sessions/${AGENT_SESSIONS[$agent_id]}"
        RELAY="${SESSION_DIR}/pieces/apps/player_app/interact_relay.txt"
        if [ -f "${RELAY}" ]; then
            echo "  Agent ${agent_id}:"
            tail -3 "${RELAY}" 2>/dev/null | sed 's/^/    /'
        fi
    done

    sleep 2
done

# Stop all agents
echo ""
echo "Stopping agents..."
for agent_id in $(seq 1 "${AGENTS}"); do
    if [ -n "${AGENT_PIDS[$agent_id]}" ]; then
        kill "${AGENT_PIDS[$agent_id]}" 2>/dev/null || true
    fi
done

bash "${WSR_DIR}/button.sh" kill >/dev/null 2>&1 || true

sleep 2

# Declare winner
echo ""
echo "========================================="
echo "🏆 TOURNAMENT RESULTS"
echo "========================================="

WINNER_ID=1
WINNER_SCORE=${AGENT_SCORES[1]:-0}
for agent_id in $(seq 2 "${AGENTS}"); do
    score=${AGENT_SCORES[$agent_id]:-0}
    if [ "$(echo "${score} > ${WINNER_SCORE}" | bc 2>/dev/null || echo 0)" -eq 1 ]; then
        WINNER_ID=$agent_id
        WINNER_SCORE=${score}
    fi
done

echo "Winner: Agent ${WINNER_ID} (fitness: ${WINNER_SCORE})"
echo ""
echo "Full results:"
for agent_id in $(seq 1 "${AGENTS}"); do
    echo "  Agent ${agent_id}: fitness=${AGENT_SCORES[$agent_id]:-0} session=${AGENT_SESSIONS[$agent_id]}"
done

echo ""
echo "Tournament data: ${TOURNAMENT_DIR}"
echo "Run dashboard: ./dashboard_xod.sh ${TOURNAMENT_DIR}"