#!/bin/bash
# button.sh — Muchi Video Editor (103.media-studio/103.vid-edit)
# Phase-1: freeglut UI + ffmpeg preview/export. See HOW2_VIDEO.md
set -e
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$SCRIPT_DIR/ops/+x/ve_main.+x"
PIDFILE="$SCRIPT_DIR/pieces/display/ve.pid"

compile() {
    mkdir -p "$SCRIPT_DIR/ops/+x" \
             "$SCRIPT_DIR/pieces/apps/player_app/media_cache" \
             "$SCRIPT_DIR/pieces/display" \
             "$SCRIPT_DIR/media"
    gcc -std=c11 -Wall -O2 -I"$SCRIPT_DIR/../shared" -o "$BIN" \
        "$SCRIPT_DIR/ops/ve_main.c" \
        "$SCRIPT_DIR/../shared/media_drop_path.c" \
        "$SCRIPT_DIR/../shared/chtpm_nav_mock.c" \
        -lglut -lGL -lGLU -lX11 -lm -lpthread -lpulse -lpulse-simple
    echo "OK $BIN"
}

case "$ACTION" in
    compile|c|build) compile ;;
    run|r|start|widget)
        compile
        if [ -f "$PIDFILE" ]; then
            old=$(cat "$PIDFILE" 2>/dev/null || true)
            if [ -n "$old" ] && kill -0 "$old" 2>/dev/null; then
                echo "Video editor already running pid=$old"
                exit 1
            fi
        fi
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        setsid "$BIN" "$SCRIPT_DIR" >/tmp/muchi-video.log 2>&1 < /dev/null &
        echo $! > "$PIDFILE"
        echo "Muchi Video started pid=$(cat "$PIDFILE")"
        echo "  log: /tmp/muchi-video.log"
        echo "  Space=play  File menu  X=export  Esc=quit"
        ;;
    kill|k|stop)
        if [ -f "$PIDFILE" ]; then
            kill "$(cat "$PIDFILE")" 2>/dev/null || true
            rm -f "$PIDFILE"
        fi
        pkill -x "ve_main.+x" 2>/dev/null || true
        echo "Video editor stopped"
        ;;
    check)
        [ -x "$BIN" ] && echo "OK binary" || echo "MISSING — compile"
        command -v ffmpeg >/dev/null && echo "OK ffmpeg" || echo "WARN no ffmpeg"
        [ -n "${DISPLAY:-}" ] && echo "OK DISPLAY" || echo "WARN no DISPLAY"
        ls "$SCRIPT_DIR/media/"*.mp4 2>/dev/null | head -5 || echo "WARN no demo media"
        ;;
    help|h|*)
        cat <<EOF
Muchi Video Editor — Phase 1 (iMovie-shaped)

  sh button.sh compile
  sh button.sh r
  sh button.sh kill
  sh button.sh check

Demo media: media/demo_*.mp4 (generated once).
EOF
        ;;
esac
