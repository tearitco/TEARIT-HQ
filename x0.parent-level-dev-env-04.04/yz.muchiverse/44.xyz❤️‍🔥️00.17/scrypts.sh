#!/bin/bash
# scrypts.sh - main dispatcher for the house's scrypts, out in the main
# dir as the single obvious entry point. Each scrypt lives in its own
# subfolder under $.crypts/scrypts/<name>/run.sh (one script per
# subfolder). Subcommands:
#
#   openall                    always-open all desired monads (no questions)
#   autostart <cmd>            forward to the autostart control
#                              (run|restart|on|off|status|compile|check|install-xdg)
#   book [cmd]                 forward to book-stack (run|window|read|kill|check)
#   muchi [cmd]                forward to muchi-pet (run|window|kill|check)
#   vvar [cmd]                 forward to hard-vvar-agent-Q0000 (run|window|read|kill|check)
#   list                       list available scrypts + what's running
#   help                       this help
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CRYPTS_DIR="$SCRIPT_DIR/$.crypts"

cmd="${1:-help}"
shift 2>/dev/null

case "$cmd" in
    openall|open|all)
        bash "$CRYPTS_DIR/scrypts/openall/run.sh" "$@"
        ;;
    autostart|auto)
        bash "$CRYPTS_DIR/scrypts/autostart/run.sh" "$@"
        ;;
    book|book-stack)
        bash "$CRYPTS_DIR/scrypts/book-stack/run.sh" "$@"
        ;;
    muchi|muchi-pet|pet)
        bash "$CRYPTS_DIR/scrypts/muchi-pet/run.sh" "$@"
        ;;
    vvar|vvarware|hard-vvar)
        bash "$CRYPTS_DIR/scrypts/vvarware/run.sh" "$@"
        ;;
    read)
        bash "$CRYPTS_DIR/scrypts/book-stack/run.sh" run
        ;;
    list|l)
        echo "Available scrypts:"
        for d in "$CRYPTS_DIR"/scrypts/*/; do
            [ -d "$d" ] || continue
            name="$(basename "$d")"
            if [ -x "$d/run.sh" ]; then
                echo "  $name"
            else
                echo "  $name (no run.sh)"
            fi
        done
        echo ""
        echo "Running window processes:"
        pgrep -af "tp_desktop_window" 2>/dev/null | sed 's/^/  /' || echo "  none"
        ;;
    help|h|-h|--help|*)
        sed -n '1,20p' "$0" | grep -E '^#   ' | sed 's/^#   /  /'
        echo ""
        echo "Examples:"
        echo "  ./scrypts.sh openall            # open every desired monad, no questions"
        echo "  ./scrypts.sh book run           # open book-stack alone (window + reader)"
        echo "  ./scrypts.sh autostart run      # run the autostart.pdl set (mount + launch)"
        echo "  ./scrypts.sh autostart off      # stop auto-opening at login"
        echo "  ./scrypts.sh list               # scrypts + running windows"
        ;;
esac
