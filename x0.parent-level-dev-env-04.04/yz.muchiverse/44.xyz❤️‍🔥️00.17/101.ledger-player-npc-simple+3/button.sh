#!/bin/bash
# button.sh - Entry point for LPNS+3 Word Game
# Supports --pal flag to use PAL script instead of C game_manager

ACTION="run"
PAL_MODE=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        run|r|start) ACTION="run" ;;
        kill|k|stop) ACTION="kill" ;;
        clean) ACTION="clean" ;;
        log) ACTION="log" ;;
        help|h|-h|--help) ACTION="help" ;;
        --pal) PAL_MODE=1 ;;
        *) ACTION="$arg" ;;
    esac
done

case "$ACTION" in
    run|r|start)
        echo "Compiling orchestrator..."
        gcc -o "$SCRIPT_DIR/system/orchestrator" "$SCRIPT_DIR/system/orchestrator.c" 2>/dev/null || {
            echo "Failed to compile orchestrator"
            exit 1
        }
        if [ "$PAL_MODE" -eq 1 ]; then
            echo "=== LPNS+3 Word Game (PAL Mode) ==="
            export PAL_LAYOUT="pieces/chtpm/layouts/lpns_word_menu_pal.chtpm"
        else
            echo "=== LPNS+3 Word Game (C Mode) ==="
            export PAL_LAYOUT=""
        fi
        exec "$SCRIPT_DIR/system/orchestrator"
        ;;
    kill|k|stop)
        pkill -f "system/orchestrator" 2>/dev/null || true
        pkill -f "game_manager" 2>/dev/null || true
        pkill -f "chtpm_parser" 2>/dev/null || true
        pkill -f "chtpm_parser_pal" 2>/dev/null || true
        pkill -f "system/renderer" 2>/dev/null || true
        pkill -f "system/keyboard_input" 2>/dev/null || true
        pkill -f "system/prisc" 2>/dev/null || true
        echo "Killed."
        ;;
    clean)
        pkill -f "system/orchestrator" 2>/dev/null || true
        pkill -f "game_manager" 2>/dev/null || true
        pkill -f "chtpm_parser" 2>/dev/null || true
        pkill -f "chtpm_parser_pal" 2>/dev/null || true
        pkill -f "system/renderer" 2>/dev/null || true
        pkill -f "system/keyboard_input" 2>/dev/null || true
        pkill -f "system/prisc" 2>/dev/null || true
        rm -f system/orchestrator system/game_manager system/keyboard_input system/renderer system/chtpm_parser system/chtpm_parser_pal system/prisc+x
        rm -f ops/word_compose_frame ops/word_turn_input ops/game_dispatch
        rm -rf data pieces/display pieces/system pieces/apps pieces/keyboard
        echo "Cleaned."
        ;;
    log)
        tail data/master_ledger.txt
        ;;
    help|h|-h|--help)
        echo "LPNS+3 Word Game Launcher"
        echo ""
        echo "Usage: ./button.sh [action] [--pal]"
        echo ""
        echo "Actions:"
        echo "  run, r, start    - Start game (default)"
        echo "  kill, k, stop    - Stop game"
        echo "  clean            - Stop game and remove all build artifacts"
        echo "  log              - Show recent ledger entries"
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
