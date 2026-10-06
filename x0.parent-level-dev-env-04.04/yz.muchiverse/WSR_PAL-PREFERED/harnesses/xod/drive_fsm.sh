#!/bin/bash
# drive_fsm.pal - Harness B: FSM Driving phase
#
# This is the SECOND harness in the chain. It:
#   1. Reads the event bus for HARNESS_DONE from setup_new_game
#   2. Decides which behavior to inject based on current state
#   3. Calls wsr_fsm_driver.+x to inject the key press
#   4. Emits HARNESS_DONE|drive|<behavior_result> to the event bus
#
# This demonstrates the "Pipeline" pattern from the XOD architecture:
#   HARNESS A (setup) → HARNESS B (drive) → HARNESS C (verify)
#
# Next harness in chain: verify_state.pal

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SESSION_DIR="${1:-.}"
cd "$SESSION_DIR"

# Read the previous harness's completion
relay="pieces/apps/player_app/interact_relay.txt"
found=""
while IFS= read -r line; do
    if echo "$line" | grep -q "^HARNESS_DONE\|setup\|ok"; then
        found="$line"
        break
    fi
done < "$relay"

if [ -z "$found" ]; then
    echo "[Harness B] No HARNESS_DONE from setup found. Exiting."
    exit 1
fi

echo "[Harness B] Starting FSM driving cycle..."

# Decide which behavior to run based on current state
# Simple strategy: check if we're at trade menu or main menu
frame="pieces/display/current_frame.txt"

# Try a sequence of behaviors for demonstration
behaviors=("end_turn" "check_market" "buy_stock" "sell_stock")

for behavior in "${behaviors[@]}"; do
    # Check if behavior preconditions are met
    case "$behavior" in
        end_turn)
            # Check if in game with active turn
            if grep -q "turn_active: true" "$frame" 2>/dev/null; then
                ./ops/+x/wsr_fsm_driver.+x "$behavior"
                echo "HARNESS_DONE|drive|${behavior}" >> "$relay"
                echo "[Harness B] Executed ${behavior}"
                exit 0
            fi
            ;;
        check_market)
            if grep -q "at_main_menu: true" "$frame" 2>/dev/null; then
                ./ops/+x/wsr_fsm_driver.+x "$behavior"
                echo "HARNESS_DONE|drive|${behavior}" >> "$relay"
                echo "[Harness B] Executed ${behavior}"
                exit 0
            fi
            ;;
        buy_stock)
            if grep -q "at_trade_menu: true" "$frame" 2>/dev/null; then
                ./ops/+x/wsr_fsm_driver.+x "$behavior"
                echo "HARNESS_DONE|drive|${behavior}" >> "$relay"
                echo "[Harness B] Executed ${behavior}"
                exit 0
            fi
            ;;
        sell_stock)
            if grep -q "in_trade_menu: true" "$frame" 2>/dev/null; then
                ./ops/+x/wsr_fsm_driver.+x "$behavior"
                echo "HARNESS_DONE|drive|${behavior}" >> "$relay"
                echo "[Harness B] Executed ${behavior}"
                exit 0
            fi
            ;;
    esac
done

# If no behavior matched, emit done
echo "HARNESS_DONE|drive|none" >> "$relay"
echo "[Harness B] No behavior matched current state."