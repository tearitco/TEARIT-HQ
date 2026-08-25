#!/bin/bash
# xo-pet-run-controller.sh - Launch XO-PET in controller-ready mode.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
CONTROLLER_UID_PAIR="${1:-}"

if [ -z "$CONTROLLER_UID_PAIR" ]; then
    CONTROLLER_UID_PAIR="controller-$(date +%s)"
fi

echo "Launching XO-PET controller mode for uid pair: $CONTROLLER_UID_PAIR"
cd "$PROJECT_ROOT"
exec bash "$PROJECT_ROOT/pieces/buttons/shared/run_orchestrator.sh" --controller "$CONTROLLER_UID_PAIR"
