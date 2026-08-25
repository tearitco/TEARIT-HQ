#!/bin/bash
# button.sh - Main launcher for CHTPM buttons (Linux/macOS)
# Usage: ./button.sh <action>

ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUTTON_DIR="$SCRIPT_DIR/pieces/buttons/linux"

case "$ACTION" in
    compile|c|build)
        echo "=== Compiling ALL binaries ==="
        bash "$SCRIPT_DIR/#.dev-storage/#.tools/compile_all.sh"
        echo ""
        echo "=== Verifying binaries ==="
        bash "$SCRIPT_DIR/#.dev-storage/#.tools/check_binaries.sh"
        ;;
    setup|deps|install)
        echo "=== Installing dependencies ==="
        bash "$SCRIPT_DIR/install_deps.sh"
        ;;
    run|r|start)
        bash "$BUTTON_DIR/run.sh"
        ;;
    kill|k|stop)
        bash "$BUTTON_DIR/kill.sh"
        ;;
    debug|d|gl)
        bash "$BUTTON_DIR/debug_gl.sh"
        ;;
    watchdog|w)
        bash "$BUTTON_DIR/watchdog.sh"
        ;;
    check|verify)
        echo "=== Verifying all binaries ==="
        bash "$SCRIPT_DIR/#.dev-storage/#.tools/check_binaries.sh"
        ;;
    help|h|-h|--help)
        echo "CHTPM Button System (Linux/macOS)"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo ""
        echo "Actions:"
        echo "  setup, deps         - Install system dependencies (macOS, Linux, MSYS2)"
        echo "  compile, c, build   - Compile ALL CHTPM binaries (projects + system)"
        echo "  run, r, start       - Run CHTPM orchestrator"
        echo "  kill, k, stop       - Kill all CHTPM processes"
        echo "  debug, d, gl        - Launch GL-OS desktop"
        echo "  watchdog, w         - Start PAL watchdog"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        echo ""
        echo "Recommended workflow:"
        echo "  1. ./button.sh compile  - Compile everything"
        echo "  2. ./button.sh check    - Verify binaries"
        echo "  3. ./button.sh run      - Start CHTPM"
        ;;
    *)
        echo "Unknown action: $ACTION"
        echo "Run './button.sh help' for usage."
        exit 1
        ;;
esac
