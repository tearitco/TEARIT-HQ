#!/bin/bash
# run.sh - Run button for Linux/macOS
# Usage: ./#.buttons/linux/run.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/../../.."
SHARED_DIR="$SCRIPT_DIR/../shared"

echo "=== Launching CHTPM (Linux) ==="
cd "$PROJECT_ROOT"
exec bash "$SHARED_DIR/run_orchestrator.sh"
