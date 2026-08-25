#!/bin/bash
# xo-pet-run-auto.sh - Launch XO-PET in autonomous mode.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

echo "Launching XO-PET in autonomous mode"
cd "$PROJECT_ROOT"
exec bash "$PROJECT_ROOT/pieces/buttons/shared/run_orchestrator.sh"
