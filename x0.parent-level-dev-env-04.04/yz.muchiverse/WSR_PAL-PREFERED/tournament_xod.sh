#!/bin/bash
# tournament_xod.sh - Multi-Agent Tournament
#
# Runs N agents simultaneously, each in an isolated session,
# competing on the same goal. Compares fitness scores and
# declares a winner.
#
# This is Phase 6 of the XOD Roadmap.
#
# Usage: ./tournament_xod.sh [agents] [generations]
#   agents: number of competing agents (default: 4)
#   generations: training generations per agent (default: 5)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
XOD_DIR="${SCRIPT_DIR}"
AGENTS="${1:-4}"
GENERATIONS="${2:-5}"

echo "=== XOD Multi-Agent Tournament ==="
echo "Agents: ${AGENTS} | Generations per agent: ${GENERATIONS}"
echo ""

# Goals all agents compete on
GOALS=("make_money" "end_turn_safe" "grow" "check_market")

declare -a AGENT_SCORES
declare -a AGENT_CHAINS

# Create temp dir for tournament results
TOURNAMENT_DIR="${XOD_DIR}/pieces/tournament_$(date +%s)"
mkdir -p "${TOURNAMENT_DIR}"

for agent_id in $(seq 1 "${AGENTS}"); do
    echo "--- Agent ${agent_id}/${AGENTS} ---"

    # Each agent gets its own session directory
    SESSION_ID="tournament_agent${agent_id}_$(date +%s)-$$"
    SESSION_DIR="${XOD_DIR}/pieces/sessions/${SESSION_ID}"

    mkdir -p "${SESSION_DIR}/pieces/system" "${SESSION_DIR}/pieces/display" \
             "${SESSION_DIR}/pieces/apps/player_app" "${SESSION_DIR}/pieces/keyboard"

    # Initialize state
    : > "${SESSION_DIR}/pieces/apps/player_app/interact_relay.txt"
    : > "${SESSION_DIR}/pieces/keyboard/history.txt"
    : > "${SESSION_DIR}/pieces/display/attrition.txt"
    : > "${SESSION_DIR}/pieces/display/current_frame.txt"
    : > "${SESSION_DIR}/pieces/display/current_layout.txt"
    : > "${SESSION_DIR}/pieces/display/fitness.txt"

    # Write mock frame state
    cat > "${SESSION_DIR}/pieces/display/current_layout.txt" << 'LAYOUT'
wsr_main_menu
LAYOUT

    cat > "${SESSION_DIR}/pieces/display/current_frame.txt" << 'FRAME'
+==== W A L L   S T R E E T   R A I D E R ====================+
|  wsr_main_menu   Turn:1   Active: corp_AFL                    |
|Your Wallet: Cash..........           500.00                   |
|Active Corp Balance Sheet: Cash (CD's)..            100.00      |
|Stock Price...           150.00                              |
+============================================================+
FRAME

    export PRISC_PROJECT_ROOT="${SESSION_DIR}"
    cd "${SESSION_DIR}"

    best_fitness=0
    best_chain=""

    # Each agent runs its own training loop
    for gen in $(seq 1 "${GENERATIONS}"); do
        for scenario in $(seq 1 2); do
            # Sample a goal
            goal_idx=$(( (agent_id + gen + scenario) % ${#GOALS[@]} ))
            goal="${GOALS[$goal_idx]}"

            # GOAP planner
            "${XOD_DIR}/ops/+x/wsr_goap_planner.+x" "${goal}" \
                "pieces/display/current_frame.txt" "${XOD_DIR}/behaviors/" 2>/dev/null || true

            # Read plan
            plan_len=0
            if [ -f "pieces/apps/player_app/plan.txt" ]; then
                plan_len=$(grep "^plan_len=" pieces/apps/player_app/plan.txt | cut -d= -f2)
            fi

            # Execute plan
            total_attrition=0
            for step in $(seq 0 $((plan_len - 1))); do
                behavior=$(grep "^step_${step}=" pieces/apps/player_app/plan.txt | cut -d= -f2)
                if [ -n "${behavior}" ]; then
                    "${XOD_DIR}/ops/+x/wsr_fsm_driver.+x" "${behavior}" 2>/dev/null || true
                    "${XOD_DIR}/ops/+x/wsr_attrition.+x" 2>/dev/null || true

                    if [ -f "pieces/display/attrition.txt" ]; then
                        cost=$(grep "^attrition_cost=" pieces/display/attrition.txt | cut -d= -f2)
                        total_attrition=$(echo "$total_attrition + $cost" | bc 2>/dev/null || echo "$total_attrition")
                    fi
                fi
            done

            # Fitness = score - attrition cost
            fitness=$(echo "100 - $total_attrition" | bc 2>/dev/null || echo "50")
            echo "  Agent ${agent_id} Gen ${gen} Scen ${scenario}: fitness=${fitness}"

            if [ "$(echo "$fitness > $best_fitness" | bc 2>/dev/null || echo 0)" -eq 1 ]; then
                best_fitness="${fitness}"
                best_chain="${behavior}"
            fi
        done

        # Evolve at end of generation
        "${XOD_DIR}/ops/+x/wsr_evolve.+x" \
            "${SESSION_DIR}/pieces/display/fitness.txt" "${XOD_DIR}/behaviors/" 2>/dev/null || true
    done

    # Record agent result
    AGENT_SCORES[$agent_id]="${best_fitness}"
    AGENT_CHAINS[$agent_id]="${best_chain}"

    # Save agent state to tournament dir
    cp "${SESSION_DIR}/pieces/display/fitness.txt" "${TOURNAMENT_DIR}/agent${agent_id}_fitness.txt" 2>/dev/null || true
    echo "  Agent ${agent_id} final fitness: ${best_fitness} | chain: ${best_chain}"
    echo ""

    # Clean up session
    rm -rf "${SESSION_DIR}"
done

# Declare winner
winner_id=1
winner_score=${AGENT_SCORES[1]}
for agent_id in $(seq 2 "${AGENTS}"); do
    if [ "$(echo "${AGENT_SCORES[$agent_id]} > $winner_score" | bc 2>/dev/null || echo 0)" -eq 1 ]; then
        winner_id=$agent_id
        winner_score=${AGENT_SCORES[$agent_id]}
    fi
done

echo "========================================="
echo "🏆 TOURNAMENT WINNER: Agent ${winner_id}"
echo "   Fitness: ${winner_score}"
echo "   Best Chain: ${AGENT_CHAINS[$winner_id]}"
echo "========================================="
echo ""
echo "Full results:"
for agent_id in $(seq 1 "${AGENTS}"); do
    echo "  Agent ${agent_id}: fitness=${AGENT_SCORES[$agent_id]} chain=${AGENT_CHAINS[$agent_id]}"
done