#!/bin/bash
# test_xod_pipeline.sh - Demo the XOD harness chaining pipeline
#
# This script demonstrates the XOD pipeline working end-to-end:
#   1. GOAP Planner (plans behaviors based on a goal)
#   2. FSM Driver (emits key presses for each behavior)
#   3. Attrition Model (tracks resource degradation)
#   4. Event Bus (all events flow through interact_relay.txt)
#
# Session isolation is handled by creating a temp session dir
# with its own pieces/ tree.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SESSION_ID="demo_$(date +%s)-$$"
SESSION_DIR="${SCRIPT_DIR}/pieces/sessions/${SESSION_ID}"

echo "=== XOD Pipeline Demo ==="
echo "Session ID: ${SESSION_ID}"

# Create session directory with pieces/ tree (xyzos-standards §23)
mkdir -p "${SESSION_DIR}/pieces/system" "${SESSION_DIR}/pieces/display" \
         "${SESSION_DIR}/pieces/apps/player_app" "${SESSION_DIR}/pieces/keyboard"

# Initialize empty state files
: > "${SESSION_DIR}/pieces/apps/player_app/interact_relay.txt"
: > "${SESSION_DIR}/pieces/keyboard/history.txt"

# Copy a mock frame for the FSM driver to read
cat > "${SESSION_DIR}/pieces/display/current_layout.txt" << 'LAYOUT'
pieces/chtpm/layouts/wsr_main_menu.chtpm
LAYOUT

cat > "${SESSION_DIR}/pieces/display/current_frame.txt" << 'FRAME'
+==== W A L L   S T R E E T   R A I D E R ====================+
+============================================================+
|  wsr_main_menu   Turn:47   Active: corp_AFL                 |
+============================================================+
|Your Wallet:|
|Cash..........           2,456.78                           |
|Active Corp Balance Sheet:|
|Cash (CD's)..            1,200.00                           |
|Stock Price...           172.50                             |
|Owned By......           (you)                             |
+============================================================+
FRAME

# Point PRISC_PROJECT_ROOT at the session dir
export PRISC_PROJECT_ROOT="${SESSION_DIR}"

cd "${SESSION_DIR}"

echo ""
echo "Step 1: GOAP Planner — goal='end_turn_safe'"
"${SCRIPT_DIR}/ops/+x/wsr_goap_planner.+x" end_turn_safe \
    "pieces/display/current_frame.txt" "${SCRIPT_DIR}/behaviors/" 2>/dev/null
echo "  → Plan written to pieces/apps/player_app/plan.txt"
cat pieces/apps/player_app/plan.txt
echo ""

echo "Step 2: FSM Driver — injecting 'end_turn' behavior"
"${SCRIPT_DIR}/ops/+x/wsr_fsm_driver.+x" end_turn 2>/dev/null
echo "  → Keyboard history:"
cat pieces/keyboard/history.txt
echo ""

echo "Step 3: Attrition Model — ticking resources"
"${SCRIPT_DIR}/ops/+x/wsr_attrition.+x" 2>/dev/null
echo "  → Attrition state:"
cat pieces/display/attrition.txt
echo ""

echo "Step 4: FSM Driver — injecting 'check_market' behavior"
"${SCRIPT_DIR}/ops/+x/wsr_fsm_driver.+x" check_market 2>/dev/null
echo "  → Keyboard history (appended):"
cat pieces/keyboard/history.txt
echo ""

echo "Step 5: Full Event Bus Contents"
echo "─────────────────────────────────────"
cat pieces/apps/player_app/interact_relay.txt
echo "─────────────────────────────────────"

echo ""
echo "=== Pipeline Demo Complete ==="
echo "All events flowed through the interact_relay.txt event bus."
echo "Check plan.txt, history.txt, attrition.txt for outputs."