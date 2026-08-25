#!/bin/bash
# run_xo-pet.sh - Legacy launcher for the XO-PET V1 PoC.

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$PROJECT_DIR/../.." && pwd)"
MANAGER_BIN="$PROJECT_DIR/pieces/manager/+x/xo-pet-v1_manager.+x"
PARSER_BIN="$ROOT_DIR/pieces/chtpm/plugins/+x/chtpm_parser.+x"
RENDERER_BIN="$ROOT_DIR/pieces/display/plugins/+x/renderer.+x"
LAYOUT="projects/xo-pet-v1/layouts/xo-pet.chtpm"
START_MODE="reset"

for arg in "$@"; do
    case "$arg" in
        reset|--reset)
            START_MODE="reset"
            ;;
        resume|--resume)
            START_MODE="resume"
            ;;
    esac
done

if [ "$START_MODE" = "reset" ]; then
    pkill -9 -f 'pieces/chtpm/plugins/.*/chtpm_parser\.\+x' 2>/dev/null || true
    pkill -9 -f 'pieces/display/plugins/.*/renderer\.\+x' 2>/dev/null || true
    pkill -9 -f 'pieces/chtpm/plugins/.*/orchestrator\.\+x' 2>/dev/null || true
    pkill -9 -f 'projects/xo-pet-v1/pieces/manager/.*/xo-pet-v1_manager\.\+x' 2>/dev/null || true
    : > "$ROOT_DIR/pieces/keyboard/history.txt"
    : > "$PROJECT_DIR/history.txt"
    : > "$PROJECT_DIR/pieces/manager/gui_state.txt"
    : > "$PROJECT_DIR/pieces/manager/manager.log"
    : > "$PROJECT_DIR/pieces/manager/active_target.txt"
    : > "$PROJECT_DIR/pieces/manager/sim_control.txt"
    : > "$PROJECT_DIR/session/state.txt"
    rm -f "$PROJECT_DIR/session"/frame_*.txt
    : > "$ROOT_DIR/pieces/display/current_frame.txt"
    : > "$ROOT_DIR/pieces/display/current_layout.txt"
    echo "Resetting XO-PET runtime scratch files"
else
    echo "Resuming XO-PET runtime scratch files"
fi

echo "Launching XO-PET V1..."
echo "Root: $ROOT_DIR"
echo "Project: $PROJECT_DIR"

if [ ! -x "$MANAGER_BIN" ]; then
    echo "ERROR: Missing manager binary: $MANAGER_BIN"
    exit 1
fi

if [ ! -x "$PARSER_BIN" ]; then
    echo "ERROR: Missing parser binary: $PARSER_BIN"
    exit 1
fi

if [ ! -x "$RENDERER_BIN" ]; then
    echo "ERROR: Missing renderer binary: $RENDERER_BIN"
    exit 1
fi

mkdir -p "$ROOT_DIR/pieces/display"
echo "$LAYOUT" > "$ROOT_DIR/pieces/display/current_layout.txt"

cd "$PROJECT_DIR"
"$MANAGER_BIN" > "$PROJECT_DIR/pieces/manager/manager.log" 2>&1 &
MGR_PID=$!
echo "Manager started (PID $MGR_PID)"

cd "$ROOT_DIR"
"$RENDERER_BIN" > "$ROOT_DIR/pieces/display/renderer.log" 2>&1 &
REN_PID=$!
echo "Renderer started (PID $REN_PID)"

trap 'kill "$MGR_PID" "$REN_PID" 2>/dev/null || true' EXIT INT TERM

cd "$ROOT_DIR"
exec "$PARSER_BIN" "$LAYOUT"
