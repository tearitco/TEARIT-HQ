#!/bin/sh
# 204.sw-battlefront — POSIX launcher
#   sh button.sh compile | run | kill | help
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DIR" || exit 1

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--std=c11 -Wall -Wextra -O2 -I./src}
LIBS="-lGLEW -lGL -lGLU -lglut -lm"
BIN="./sw_battlefront"
SRC="src/main.c src/gen.c src/gfx.c src/sim.c src/ui.c"
PIDFILE="./.sw_battlefront.pid"

cmd=${1:-help}

case "$cmd" in
  compile|c|build)
    echo "=== sw-battlefront: compile ==="
    # shellcheck disable=SC2086
    $CC $CFLAGS -o "$BIN" $SRC $LIBS
    echo "OK: $BIN"
    ;;
  run|r|start)
    if [ -z "${DISPLAY:-}" ]; then
      echo "No DISPLAY — cannot open freeglut window"
      exit 1
    fi
    if [ ! -x "$BIN" ]; then
      echo "binary missing — compiling first..."
      # shellcheck disable=SC2086
      $CC $CFLAGS -o "$BIN" $SRC $LIBS
    fi
    echo "=== sw-battlefront: run ==="
    echo "  Menu: Supremacy / Deathmatch / Freeplay"
    echo "  Q menu · mouse look · WASD · LMB fire · E ship"
    if [ "${FOREGROUND:-0}" = "1" ]; then
      exec "$BIN"
    fi
    "$BIN" &
    echo $! > "$PIDFILE"
    echo "pid $(cat "$PIDFILE")  (sh button.sh kill to stop)"
    wait "$(cat "$PIDFILE")" 2>/dev/null || true
    rm -f "$PIDFILE"
    ;;
  kill|k|stop)
    echo "=== sw-battlefront: kill ==="
    if [ -f "$PIDFILE" ]; then
      pid=$(cat "$PIDFILE")
      kill "$pid" 2>/dev/null || true
      sleep 0.15
      kill -9 "$pid" 2>/dev/null || true
      rm -f "$PIDFILE"
      echo "killed pid $pid"
    fi
    pkill -x sw_battlefront 2>/dev/null || true
    echo "done"
    ;;
  help|h|-h|--help|*)
    cat <<EOF
204.sw-battlefront — Star Wars Battlefront-style freeglut clone

  sh button.sh compile   build ./sw_battlefront
  sh button.sh run       compile if needed, run
  sh button.sh kill      stop running instance
  sh button.sh help

Modes:
  SUPREMACY   capture posts, ticket bleed, team AI
  DEATHMATCH  space dogfight, frag limit 25
  FREEPLAY    planets, mine/build, ships, saber, O2

Deps: gcc, freeglut, libGL, libGLU, libGLEW
Env:  CC CFLAGS FOREGROUND=1
EOF
    if [ "$cmd" = "help" ] || [ "$cmd" = "h" ] || [ "$cmd" = "-h" ] || [ "$cmd" = "--help" ]; then
      exit 0
    fi
    exit 1
    ;;
esac
