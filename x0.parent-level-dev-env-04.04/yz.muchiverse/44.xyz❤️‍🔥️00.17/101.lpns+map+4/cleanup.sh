#!/bin/sh
# cleanup.sh - Kill all LPNS+MAP+3 processes and reset state
# This script can be sourced or run directly
# Can be used as Ctrl+C handler or called manually

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_ROOT"

echo "Killing LPNS+MAP+3 processes..."

# Kill renderer, keyboard_input, and prisc+x
pkill -f "system/renderer" 2>/dev/null || true
pkill -f "system/keyboard_input" 2>/dev/null || true
pkill -f "prisc\+x" 2>/dev/null || true

# Reset state files
rm -f pieces/system/quit_flag.txt
rm -f pieces/display/current_frame.txt
rm -f pieces/apps/player_app/interact_relay.txt

echo "Cleanup complete."
