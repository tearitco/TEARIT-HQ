#!/bin/sh
# button.sh — POSIX compile | run | kill | help for glut-craft
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DIR" || exit 1

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--std=c11 -Wall -Wextra -O2 -I./src}
LIBS="-lGL -lGLU -lglut -lm"
BIN="./glut-craft"
SRC="src/main.c src/world.c src/player.c src/render.c src/inv.c"
PIDFILE="./.glut-craft.pid"

cmd=${1:-help}

case "$cmd" in
  compile|c|build)
    echo "=== glut-craft: compile ==="
    # shellcheck disable=SC2086
    $CC $CFLAGS -o "$BIN" $SRC $LIBS
    echo "OK: $BIN"
    ;;
  run|r|start)
    if [ ! -x "$BIN" ]; then
      echo "binary missing — compiling first..."
      # shellcheck disable=SC2086
      $CC $CFLAGS -o "$BIN" $SRC $LIBS
    fi
    echo "=== glut-craft: run ==="
    # background + pid so kill works; still foreground-ish via wait if TTY
    "$BIN" &
    echo $! > "$PIDFILE"
    echo "pid $(cat "$PIDFILE") — click window to capture mouse; Q quit"
    wait "$(cat "$PIDFILE")" 2>/dev/null || true
    rm -f "$PIDFILE"
    ;;
  kill|k|stop)
    echo "=== glut-craft: kill ==="
    if [ -f "$PIDFILE" ]; then
      pid=$(cat "$PIDFILE")
      kill "$pid" 2>/dev/null || true
      rm -f "$PIDFILE"
      echo "killed pid $pid"
    fi
    # also by name (best-effort)
    pkill -f "$DIR/glut-craft" 2>/dev/null || true
    pkill -x glut-craft 2>/dev/null || true
    echo "done"
    ;;
  help|h|-h|--help|*)
    cat <<EOF
glut-craft button.sh (POSIX sh)

  sh button.sh compile   # build ./glut-craft
  sh button.sh run       # compile if needed, run game
  sh button.sh kill      # stop running instance
  sh button.sh help      # this text

Deps: gcc, freeglut3-dev (or equiv), libgl, libglu
Env:  CC CFLAGS
EOF
    if [ "$cmd" = "help" ] || [ "$cmd" = "h" ] || [ "$cmd" = "-h" ] || [ "$cmd" = "--help" ]; then
      exit 0
    fi
    exit 1
    ;;
esac
