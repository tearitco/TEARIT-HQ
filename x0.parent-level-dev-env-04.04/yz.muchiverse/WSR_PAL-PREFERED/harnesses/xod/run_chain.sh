#!/bin/bash
# harnesses/run_chain.sh - Execute the XOD harness chain
#
# Usage: ./run_chain.sh [session_dir]
#
# Runs:
#   1. setup_new_game.sh  (Harness A)
#   2. drive_fsm.sh       (Harness B)
#   3. verify_state.sh    (Harness C)
#
# Each harness writes HARNESS_DONE|<name>|result to interact_relay.txt
# The next harness reads that event before starting.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SESSION_DIR="${1:-.}"
cd "$SESSION_DIR"

HARNESS_DIR="$SCRIPT_DIR"
RELAY="pieces/apps/player_app/interact_relay.txt"

echo "=== XOD Harness Chain ==="

# Harness A: Setup
echo "[1/3] Setup new game..."
bash "$HARNESS_DIR/setup_new_game.sh" "$SESSION_DIR"

# Wait for HARNESS_DONE|setup
timeout=10
while [ ! -s "$RELAY" ] && [ $timeout -gt 0 ]; do
    sleep 1
    timeout=$((timeout - 1))
done

grep -q "HARNESS_DONE|setup" "$RELAY" || echo "Warning: setup HARNESS_DONE not found"

# Harness B: Drive FSM
echo "[2/3] Drive FSM..."
bash "$HARNESS_DIR/drive_fsm.sh" "$SESSION_DIR"

timeout=10
while [ ! -s "$RELAY" ] && [ $timeout -gt 0 ]; do
    sleep 1
    timeout=$((timeout - 1))
done

grep -q "HARNESS_DONE|drive" "$RELAY" || echo "Warning: drive HARNESS_DONE not found"

# Harness C: Verify State
echo "[3/3] Verify state..."
bash "$HARNESS_DIR/verify_state.sh" "$SESSION_DIR"

timeout=10
while [ ! -s "$RELAY" ] && [ $timeout -gt 0 ]; do
    sleep 1
    timeout=$((timeout - 1))
done

grep -q "HARNESS_DONE|verify" "$RELAY" || echo "Warning: verify HARNESS_DONE not found"

echo "=== Harness Chain Complete ==="
echo "Event bus:"
cat "$RELAY"