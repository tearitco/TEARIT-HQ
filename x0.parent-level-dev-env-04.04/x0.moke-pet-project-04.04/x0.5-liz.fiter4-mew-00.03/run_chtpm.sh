#!/bin/bash
# run_chtpm.sh - Template-only clone launcher

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
exec bash "$SCRIPT_DIR/pieces/buttons/shared/run_orchestrator.sh"
