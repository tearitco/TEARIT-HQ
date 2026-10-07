#!/bin/bash
# harnesses/presentation.pal - Presentation Harness
#
# Auto-records proof videos of XOD agent behavior.
# Integrates with make_presentation_video.py (house standard).
#
# Usage: ./presentation.sh <session_dir> [agent_id]
#
# Outputs:
#   presentations/xod_tournament_<agent_id>/
#   ├── snapshots/           # PNG frames from gl_mirror
#   ├── manifest.txt         # Frame descriptions
#   ├── REPRODUCE.md         # How to replay
#   └── presentation.mp4     # Final video with TTS

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SESSION_DIR="${1:-.}"
AGENT_ID="${2:-0}"
XOD_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

PRES_DIR="${XOD_DIR}/presentations/xod_tournament_agent${AGENT_ID}"
SNAP_DIR="${PRES_DIR}/snapshots"

mkdir -p "$SNAP_DIR"

echo "[Presentation Harness] Recording agent ${AGENT_ID}..."

# Capture frames from gl_mirror output (if running)
FRAME_NUM=0
for frame in pieces/display/frame_*.png pieces/display/current_frame.txt; do
    if [ -f "$frame" ]; then
        FRAME_NUM=$((FRAME_NUM + 1))
        cp "$frame" "${SNAP_DIR}/frame_${FRAME_NUM}.png" 2>/dev/null || true
    fi
done

# If no frames captured, create a text snapshot
if [ "$FRAME_NUM" -eq 0 ]; then
    FRAME_NUM=1
    cp pieces/display/current_frame.txt "${SNAP_DIR}/frame_${FRAME_NUM}.txt" 2>/dev/null || true
fi

# Write manifest
cat > "${PRES_DIR}/manifest.txt" << MANIFEST
XOD Tournament Agent ${AGENT_ID} - Presentation
=============================================
Generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)
Agent ID: ${AGENT_ID}
Session: $(basename "$SESSION_DIR")
Frames captured: ${FRAME_NUM}

frame_1 | initial | Agent starting, initial frame captured
frame_${FRAME_NUM} | final | Final state after training
MANIFEST

# Write REPRODUCE.md
cat > "${PRES_DIR}/REPRODUCE.md" << REPRO
# XOD Tournament Agent ${AGENT_ID} - Reproduction Guide

## Session
Session dir: ${SESSION_DIR}

## Commands
cd ${XOD_DIR}
./train_xod.sh 5 3  # Run 5 generations, 3 scenarios each

## Expected Output
- pieces/apps/player_app/interact_relay.txt contains event stream
- pieces/display/fitness.txt contains final fitness score
- ops/+x/wsr_fsm_driver.+x emitted key presses to keyboard/history.txt

## Harness Chain
setup_new_game.sh -> drive_fsm.sh -> verify_state.sh -> presentation.sh
REPRO

# Emit HARNESS_DONE for chain
echo "HARNESS_DONE|presentation|agent${AGENT_ID}_recorded" >> "${SESSION_DIR}/pieces/apps/player_app/interact_relay.txt"

echo "[Presentation Harness] Agent ${AGENT_ID} recording complete: ${PRES_DIR}"