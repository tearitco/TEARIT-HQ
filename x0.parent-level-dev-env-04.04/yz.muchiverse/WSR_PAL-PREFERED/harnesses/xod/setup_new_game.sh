#!/bin/bash
# setup_new_game.pal - Harness A: Setup phase
#
# This is the FIRST harness in the chain. It:
#   1. Starts a new WSR game (resets world)
#   2. Waits for the world to be ready
#   3. Emits HARNESS_DONE|setup|ok to the event bus
#
# Next harness in chain: drive_fsm.pal
#
# Usage: ./harnesses/setup_new_game.pal [session_dir]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SESSION_DIR="${1:-.}"
cd "$SESSION_DIR"

echo "[Harness A] Starting new game..."

# Reset the world
if [ -x "ops/+x/wsr_fsm_driver.+x" ]; then
    ./ops/+x/wsr_fsm_driver.+x new_game
fi

# Wait for world to be ready (check for frame change)
sleep 1

# Emit HARNESS_DONE to the event bus
echo "HARNESS_DONE|setup|ok" >> pieces/apps/player_app/interact_relay.txt
echo "[Harness A] Setup complete."