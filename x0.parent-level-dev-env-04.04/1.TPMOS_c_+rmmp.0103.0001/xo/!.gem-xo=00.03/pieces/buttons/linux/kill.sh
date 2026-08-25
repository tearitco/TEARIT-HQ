#!/bin/bash
# kill.sh - Kill button for Linux/macOS
# Usage: ./pieces/buttons/linux/kill.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/../../.."

echo "=== Killing CHTPM Processes (Linux) ==="
cd "$PROJECT_ROOT"
bash "$PROJECT_ROOT/pieces/os/kill_all.sh"
