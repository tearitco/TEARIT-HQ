#!/bin/bash
# train_xod.sh - XOD Evolutionary Training Loop (with real fitness)
#
# Implements the training loop from XOD Architecture §9:
#
#   for generation in 1..N:
#       for scenario in scenarios:
#           session = new_session()
#           goal = sample_goal()
#           plan = goap.plan(goal, session.state)
#
#           for behavior in plan:
#               result = session.run(behavior)
#               attrition.tick()
#               score += goap.score(behavior, result)
#
#           fitness = wsr_fitness(session.corp_state)
#           population.evolve(fitness)
#
# NOW WITH REAL FITNESS: reads corp_ORB/state.txt via wsr_fitness.+x

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
XOD_DIR="${SCRIPT_DIR}"
GENERATIONS="${1:-10}"
SCENARIOS="${2:-5}"
SESSION_BASE="${XOD_DIR}/pieces/sessions"

echo "=== XOD Training Loop (Real Fitness) ==="
echo "Generations: ${GENERATIONS} | Scenarios per gen: ${SCENARIOS}"

GOALS=("make_money" "end_turn_safe" "grow" "check_market")

# Shared fitness tracking for evolution
FITNESS_FILE="${XOD_DIR}/pieces/display/fitness_history.txt"
: > "${FITNESS_FILE}"

for gen in $(seq 1 "${GENERATIONS}"); do
    echo ""
    echo "--- Generation ${gen}/${GENERATIONS} ---"

    for scenario in $(seq 1 "${SCENARIOS}"); do
        SESSION_ID="gen${gen}_scen${scenario}_$(date +%s)-$$"
        SESSION_DIR="${SESSION_BASE}/${SESSION_ID}"

        mkdir -p "${SESSION_DIR}/pieces/system" "${SESSION_DIR}/pieces/display" \
                 "${SESSION_DIR}/pieces/apps/player_app" "${SESSION_DIR}/pieces/keyboard" \
                 "${SESSION_DIR}/projects/wsr-pal/pieces/corp_ORB"

        # Initialize event bus and state
        : > "${SESSION_DIR}/pieces/apps/player_app/interact_relay.txt"
        : > "${SESSION_DIR}/pieces/keyboard/history.txt"
        : > "${SESSION_DIR}/pieces/display/attrition.txt"
        : > "${SESSION_DIR}/pieces/display/current_frame.txt"
        : > "${SESSION_DIR}/pieces/display/current_layout.txt"

        # Copy template corp state (real seed data)
        if [ -f "${XOD_DIR}/projects/wsr-pal/pieces_template/corp_ORB/state.txt" ]; then
            cp "${XOD_DIR}/projects/wsr-pal/pieces_template/corp_ORB/state.txt" \
               "${SESSION_DIR}/projects/wsr-pal/pieces/corp_ORB/state.txt"
        fi
        if [ -f "${XOD_DIR}/projects/wsr-pal/pieces_template/corp_ORB/price_history.txt" ]; then
            cp "${XOD_DIR}/projects/wsr-pal/pieces_template/corp_ORB/price_history.txt" \
               "${SESSION_DIR}/projects/wsr-pal/pieces/corp_ORB/price_history.txt"
        fi

        # Write mock frame
        cat > "${SESSION_DIR}/pieces/display/current_layout.txt" << 'LAYOUT'
wsr_main_menu
LAYOUT

        cat > "${SESSION_DIR}/pieces/display/current_frame.txt" << 'FRAME'
+==== W A L L   S T R E E T   R A I D E R ====================+
|  wsr_main_menu   Turn:1   Active: corp_ORB                   |
|Your Wallet: Cash..........           500.00                  |
|Active Corp Balance Sheet: Cash (CD's)..            100.00    |
|Stock Price...           150.00                             |
+============================================================+
FRAME

        # Sample a goal (rotate for diversity)
        goal_idx=$(( (scenario + gen) % ${#GOALS[@]} ))
        goal="${GOALS[$goal_idx]}"

        echo "  Scenario ${scenario}/${SCENARIOS}: goal='${goal}'"

        export PRISC_PROJECT_ROOT="${SESSION_DIR}"
        cd "${SESSION_DIR}"

        # Step 1: GOAP planner
        "${XOD_DIR}/ops/+x/wsr_goap_planner.+x" "${goal}" \
            "pieces/display/current_frame.txt" "${XOD_DIR}/behaviors/" 2>/dev/null || true

        # Step 2: Execute plan
        plan_len=0
        if [ -f "pieces/apps/player_app/plan.txt" ]; then
            plan_len=$(grep "^plan_len=" pieces/apps/player_app/plan.txt | cut -d= -f2)
        fi

        for step in $(seq 0 $((plan_len - 1))); do
            behavior=$(grep "^step_${step}=" pieces/apps/player_app/plan.txt | cut -d= -f2)
            if [ -n "${behavior}" ]; then
                "${XOD_DIR}/ops/+x/wsr_fsm_driver.+x" "${behavior}" 2>/dev/null || true
                "${XOD_DIR}/ops/+x/wsr_attrition.+x" 2>/dev/null || true
            fi
        done

        # Step 3: REAL FITNESS from corp_ORB state
        "${XOD_DIR}/ops/+x/wsr_fitness.+x" \
            "projects/wsr-pal/pieces/corp_ORB/state.txt" \
            "projects/wsr-pal/pieces/corp_ORB/price_history.txt" 2>/dev/null || true

        # Read fitness
        fitness="0"
        if [ -f "pieces/display/fitness.txt" ]; then
            fitness=$(grep "^fitness=" pieces/display/fitness.txt | cut -d= -f2)
        fi

        echo "    fitness=${fitness}"
        echo "gen${gen}_scen${scenario} fitness=${fitness} chain=${behavior}" >> "${FITNESS_FILE}"

        # Step 4: Evolve at end of each generation
        if [ "${scenario}" -eq "${SCENARIOS}" ]; then
            "${XOD_DIR}/ops/+x/wsr_evolve.+x" \
                "${FITNESS_FILE}" "${XOD_DIR}/behaviors/" 2>/dev/null || true
            echo "  Evolution complete for generation ${gen}"
        fi

        # Clean up session
        rm -rf "${SESSION_DIR}"
    done
done

echo ""
echo "=== Training Complete ==="
echo "Fitness history:"
tail -20 "${FITNESS_FILE}"