#!/bin/bash
# button.sh — Muchi Blend (103.media-studio/13.3d=blender-clone)
# Phase-1: freeglut Blender-shaped 3D viewport. OBJ/FBX via Assimp.
# See HOW2_BLEND.md
set -e
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$SCRIPT_DIR/ops/+x/be_main.+x"
PIDFILE="$SCRIPT_DIR/pieces/display/be.pid"

compile() {
    mkdir -p "$SCRIPT_DIR/ops/+x" \
             "$SCRIPT_DIR/pieces/apps/player_app" \
             "$SCRIPT_DIR/pieces/display" \
             "$SCRIPT_DIR/media"
    ASSIMP_CFLAGS="$(pkg-config --cflags assimp 2>/dev/null || true)"
    ASSIMP_LIBS="$(pkg-config --libs assimp 2>/dev/null || echo -lassimp)"
    gcc -std=c11 -Wall -O2 -I"$SCRIPT_DIR/../shared" $ASSIMP_CFLAGS -o "$BIN" \
        "$SCRIPT_DIR/ops/be_main.c" \
        "$SCRIPT_DIR/../shared/media_drop_path.c" \
        "$SCRIPT_DIR/../shared/chtpm_nav_mock.c" \
        -lglut -lGL -lGLU -lX11 -lm -lpthread $ASSIMP_LIBS
    echo "OK $BIN"
}

case "$ACTION" in
    compile|c|build) compile ;;
    run|r|start|widget)
        compile
        if [ -f "$PIDFILE" ]; then
            old=$(cat "$PIDFILE" 2>/dev/null || true)
            if [ -n "$old" ] && kill -0 "$old" 2>/dev/null; then
                echo "Blend already running pid=$old"
                exit 1
            fi
        fi
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        setsid "$BIN" "$SCRIPT_DIR" >/tmp/muchi-blend.log 2>&1 < /dev/null &
        echo $! > "$PIDFILE"
        echo "Muchi Blend started pid=$(cat "$PIDFILE")"
        echo "  log: /tmp/muchi-blend.log"
        echo "  MMB orbit · G/R/S transform · drop .obj/.fbx · Esc quit"
        ;;
    kill|k|stop)
        if [ -f "$PIDFILE" ]; then
            kill "$(cat "$PIDFILE")" 2>/dev/null || true
            rm -f "$PIDFILE"
        fi
        pkill -x "be_main.+x" 2>/dev/null || true
        echo "Blend stopped"
        ;;
    check)
        [ -x "$BIN" ] && echo "OK binary" || echo "MISSING — compile"
        pkg-config --exists assimp && echo "OK assimp" || echo "WARN no assimp (OBJ/FBX import)"
        [ -n "${DISPLAY:-}" ] && echo "OK DISPLAY" || echo "WARN no DISPLAY"
        ;;
    help|h|*)
        cat <<EOF
Muchi Blend — Phase 1 (Blender-shaped 3D)

  sh button.sh compile
  sh button.sh r
  sh button.sh kill
  sh button.sh check

Drop .obj / .fbx onto viewport. MMB orbit, scroll zoom, G/R/S move/rotate/scale.
EOF
        ;;
esac
