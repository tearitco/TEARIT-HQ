#!/bin/bash
# button.sh — Muchi Image (103.media-studio/103.img-editor)
# Phase-1: freeglut Photoshop-shaped raster editor. See HOW2_IMAGE.md
set -e
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$SCRIPT_DIR/ops/+x/ie_main.+x"
PIDFILE="$SCRIPT_DIR/pieces/display/ie.pid"

compile() {
    mkdir -p "$SCRIPT_DIR/ops/+x" \
             "$SCRIPT_DIR/pieces/apps/player_app" \
             "$SCRIPT_DIR/pieces/display" \
             "$SCRIPT_DIR/media"
    gcc -std=c11 -Wall -O2 -I"$SCRIPT_DIR/../shared" -o "$BIN" \
        "$SCRIPT_DIR/ops/ie_main.c" \
        "$SCRIPT_DIR/../shared/media_drop_path.c" \
        "$SCRIPT_DIR/../shared/chtpm_nav_mock.c" \
        -lglut -lGL -lGLU -lX11 -lm -lpthread
    echo "OK $BIN"
}

case "$ACTION" in
    compile|c|build) compile ;;
    run|r|start|widget)
        compile
        if [ -f "$PIDFILE" ]; then
            old=$(cat "$PIDFILE" 2>/dev/null || true)
            if [ -n "$old" ] && kill -0 "$old" 2>/dev/null; then
                echo "Image editor already running pid=$old"
                exit 1
            fi
        fi
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        setsid "$BIN" "$SCRIPT_DIR" >/tmp/muchi-image.log 2>&1 < /dev/null &
        echo $! > "$PIDFILE"
        echo "Muchi Image started pid=$(cat "$PIDFILE")"
        echo "  log: /tmp/muchi-image.log"
        echo "  B=brush E=eraser G=fill R=rect I=eyedrop [ ] size X swap  Esc=quit"
        ;;
    kill|k|stop)
        if [ -f "$PIDFILE" ]; then
            kill "$(cat "$PIDFILE")" 2>/dev/null || true
            rm -f "$PIDFILE"
        fi
        pkill -x "ie_main.+x" 2>/dev/null || true
        echo "Image editor stopped"
        ;;
    check)
        [ -x "$BIN" ] && echo "OK binary" || echo "MISSING — compile"
        command -v ffmpeg >/dev/null && echo "OK ffmpeg" || echo "WARN no ffmpeg (import/export limited)"
        [ -n "${DISPLAY:-}" ] && echo "OK DISPLAY" || echo "WARN no DISPLAY"
        ;;
    help|h|*)
        cat <<EOF
Muchi Image — Phase 1 (Photoshop-shaped)

  sh button.sh compile
  sh button.sh r
  sh button.sh kill
  sh button.sh check

Drop PNG/JPG onto canvas. Tools: B E G R I. Layers 1-6. Ctrl+S save.
EOF
        ;;
esac
