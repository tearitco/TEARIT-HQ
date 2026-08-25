#!/bin/bash
# button.sh - book-stack launcher (standalone open).
# book-stack is a MONAD: *.monads/*.book-stack/entities/book-stack is the
# window (hosted by tp_desktop_window), and its Read method runs the
# reader app (*.monads/*.book-stack/pieces/reader/event_pkg/pages/page_1/
# event.pal) via 101.mutaclsym.../system/prisc+x. Opening book-stack on
# its own therefore means: start the entity window, then start the
# reader so the Choose-Read/Hear/Tao dispatch appears.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

TPWIN="$HOUSE_DIR/*.monads/*.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x"
PRISC="$HOUSE_DIR/101.mutaclsym🧟‍♂️️+18.01/system/prisc+x"
ENTITY_DIR="$HOUSE_DIR/*.monads/*.book-stack/entities/book-stack"
EVENT_PAL="$SCRIPT_DIR/pieces/reader/event_pkg/pages/page_1/event.pal"

entity_window_up() {
    # escape regex specials — monad paths contain literal '*' globs
    # (*.monads/...) that pgrep -f would otherwise treat as metachars
    local re
    re=$(printf '%s' "$ENTITY_DIR" | sed 's/[][{}.*+?^$|\\]/\\&/g')
    pgrep -f "$re" >/dev/null 2>&1
}

start_entity_window() {
    if entity_window_up; then
        echo "book-stack window already open"
        return 0
    fi
    if [ ! -x "$TPWIN" ]; then
        echo "MISSING tp_desktop_window binary: $TPWIN"
        return 1
    fi
    setsid nohup "$TPWIN" "$ENTITY_DIR" >/dev/null 2>&1 &
    sleep 1
    echo "book-stack window started"
}

case "$ACTION" in
    run|r|start)
        start_entity_window
        if [ -x "$PRISC" ]; then
            exec "$PRISC" "$EVENT_PAL"
        else
            echo "MISSING prisc+x: $PRISC"
            exit 1
        fi
        ;;
    window|w)
        start_entity_window
        ;;
    read|open)
        if entity_window_up; then
            [ -x "$PRISC" ] && exec "$PRISC" "$EVENT_PAL" \
                || { echo "MISSING prisc+x: $PRISC"; exit 1; }
        else
            start_entity_window
            sleep 1
            [ -x "$PRISC" ] && exec "$PRISC" "$EVENT_PAL" \
                || { echo "MISSING prisc+x: $PRISC"; exit 1; }
        fi
        ;;
    kill|k|stop)
        local re
        re=$(printf '%s' "$ENTITY_DIR" | sed 's/[][{}.*+?^$|\\]/\\&/g')
        pkill -f "$re" 2>/dev/null
        echo "book-stack closed"
        ;;
    check|verify)
        for b in "$TPWIN" "$PRISC"; do
            [ -x "$b" ] && echo "OK   $b" || echo "MISSING $b"
        done
        [ -d "$ENTITY_DIR" ] && echo "OK   entity $ENTITY_DIR" || echo "MISSING entity $ENTITY_DIR"
        [ -f "$EVENT_PAL" ] && echo "OK   $EVENT_PAL" || echo "MISSING $EVENT_PAL"
        ;;
    help|h|-h|--help|*)
        cat <<'EOF'
book-stack launcher

  sh button.sh run       # open book-stack (entity window + reader dispatch)
  sh button.sh window    # open just the book-stack entity window
  sh button.sh read      # open the reader dispatch (window must be up)
  sh button.sh kill      # close the book-stack window
  sh button.sh check     # verify binaries/paths
  sh button.sh help      # this help
EOF
        ;;
esac
