#!/bin/bash
# tournament_setup.sh — Launch XOD Tournament Setup Screen
#
# Opens the CHTPM-based setup UI to configure agent slots,
# then launches the tournament.
#
# Usage: ./tournament_setup.sh [session_dir]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
XOD_DIR="${SCRIPT_DIR}"
SESSION_DIR="${1:-}"

if [ -z "$SESSION_DIR" ]; then
    SESSION_ID="setup_$(date +%s)-$$"
    SESSION_DIR="${XOD_DIR}/pieces/sessions/${SESSION_ID}"
fi

echo "=== XOD Tournament Setup ==="
echo "Session: ${SESSION_DIR}"

# Create session directory structure
mkdir -p "${SESSION_DIR}/pieces/system" "${SESSION_DIR}/pieces/display" \
         "${SESSION_DIR}/pieces/apps/player_app" "${SESSION_DIR}/pieces/keyboard" \
         "${SESSION_DIR}/pieces/chtpm" "${SESSION_DIR}/projects/wsr-pal/pieces/corp_ORB" \
         "${SESSION_DIR}/debug"

# Initialize state files
: > "${SESSION_DIR}/pieces/apps/player_app/interact_relay.txt"
: > "${SESSION_DIR}/pieces/keyboard/history.txt"
: > "${SESSION_DIR}/pieces/display/current_frame.txt"
: > "${SESSION_DIR}/pieces/display/current_layout.txt"
: > "${SESSION_DIR}/pieces/display/frame_changed.txt"
: > "${SESSION_DIR}/pieces/display/active_gui_index.txt"
: > "${SESSION_DIR}/pieces/display/active_gui_is_typing.txt"
: > "${SESSION_DIR}/debug/frame_history.txt"

# Initialize setup state with defaults
cat > "${SESSION_DIR}/pieces/apps/player_app/xod_setup_state.txt" << EOF
num_agents=1
duration=60
agent_1_type=llm
agent_1_model=gemma3:1b
agent_1_goal=survive
agent_1_temp=0.3
EOF

# Copy template corp state
if [ -f "${XOD_DIR}/projects/wsr-pal/pieces_template/corp_ORB/state.txt" ]; then
    mkdir -p "${SESSION_DIR}/projects/wsr-pal/pieces/corp_ORB"
    cp "${XOD_DIR}/projects/wsr-pal/pieces_template/corp_ORB/state.txt" \
       "${SESSION_DIR}/projects/wsr-pal/pieces/corp_ORB/state.txt"
fi
if [ -f "${XOD_DIR}/projects/wsr-pal/pieces_template/corp_ORB/price_history.txt" ]; then
    cp "${XOD_DIR}/projects/wsr-pal/pieces_template/corp_ORB/price_history.txt" \
       "${SESSION_DIR}/projects/wsr-pal/pieces/corp_ORB/price_history.txt"
fi

# Set layout
echo "pieces/chtpm/layouts/wsr_xod_setup.chtpm" > "${SESSION_DIR}/pieces/display/current_layout.txt"

# Export PRISC variables
export PRISC_PROJECT_ROOT="${SESSION_DIR}"
export PRISC_PROJECT_ID="wsr-pal"
export PAL_LAYOUT="pieces/chtpm/layouts/wsr_xod_setup.chtpm"

echo "Starting setup screen..."
echo "Configure agent slots, then select 'Start Tournament'"
echo ""

cd "${SESSION_DIR}"
exec "${XOD_DIR}/system/orchestrator"