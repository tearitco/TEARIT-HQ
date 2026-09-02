#!/bin/bash
# button.sh - Entry point for LPNS+MAP+4
# Supports --pal flag to use PAL script instead of C game_manager

ACTION="run"
PAL_MODE=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        run|r|start) ACTION="run" ;;
        kill|k|stop) ACTION="kill" ;;
        help|h|-h|--help) ACTION="help" ;;
        --pal) PAL_MODE=1 ;;
        *) ACTION="$arg" ;;
    esac
done

case "$ACTION" in
    run|r|start)
        if [ "$PAL_MODE" -eq 1 ]; then
            echo "=== LPNS+MAP+4 Launcher (PAL Mode) ==="
            export PAL_LAYOUT="pieces/chtpm/layouts/lpns_main_menu_pal.chtpm"
        else
            echo "=== LPNS+MAP+4 Launcher (C Mode) ==="
            export PAL_LAYOUT=""
        fi
        exec "$SCRIPT_DIR/system/orchestrator"
        ;;
    kill|k|stop)
        pkill -f "system/orchestrator" 2>/dev/null || true
        pkill -f "game_manager" 2>/dev/null || true
        pkill -f "chtpm_parser_pal" 2>/dev/null || true
        pkill -f "system/renderer" 2>/dev/null || true
        pkill -f "system/keyboard_input" 2>/dev/null || true
        pkill -f "system/prisc" 2>/dev/null || true
        echo "Killed."
        ;;
    help|h|-h|--help)
        echo "LPNS+MAP+4 Launcher"
        echo ""
        echo "Usage: ./button.sh [action] [--pal]"
        echo ""
        echo "Actions:"
        echo "  run, r, start    - Start game (default)"
        echo "  kill, k, stop    - Stop game"
        echo "  help, h          - Show this help"
        echo ""
        echo "Options:"
        echo "  --pal            - Use PAL script instead of C game_manager"
        echo ""
        echo "Examples:"
        echo "  ./button.sh run        # Start with C game_manager"
        echo "  ./button.sh run --pal  # Start with PAL script"
        echo "  ./button.sh --pal run  # Same as above"
        ;;
    *)
        echo "Unknown action: $ACTION"
        echo "Run './button.sh help' for usage."
        exit 1
        ;;
esac
