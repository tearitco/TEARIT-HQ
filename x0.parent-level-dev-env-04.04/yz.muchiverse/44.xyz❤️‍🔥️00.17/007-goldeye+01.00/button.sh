#!/bin/sh
# 007-goldeye-clysim — freeglut voxel split-screen GoldenEye-ish
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DIR" || exit 1
BIN=./goldeye_clysim
PIDFILE=./.goldeye.pid
SRC="src/main.c"
CC=${CC:-gcc}
CFLAGS=${CFLAGS:--std=c11 -Wall -Wextra -O2}
LIBS="-lGL -lGLU -lglut -lm"

cmd=${1:-help}
case "$cmd" in
  compile|c|build)
    echo "=== goldeye-clysim compile ==="
    # shellcheck disable=SC2086
    $CC $CFLAGS -o "$BIN" $SRC $LIBS
    echo "OK $BIN"
    ;;
  run|r)
    if [ -z "${DISPLAY:-}" ]; then echo "No DISPLAY"; exit 1; fi
    if [ ! -x "$BIN" ]; then $CC $CFLAGS -o "$BIN" $SRC $LIBS; fi
    echo "run $BIN  (Esc menu, P pause, 1/2 cam, S reset, Ctrl+C quit)"
    if [ "${FOREGROUND:-0}" = "1" ]; then exec "$BIN"; fi
    "$BIN" &
    echo $! > "$PIDFILE"
    wait "$(cat "$PIDFILE")" 2>/dev/null || true
    rm -f "$PIDFILE"
    ;;
  kill|k)
    if [ -f "$PIDFILE" ]; then
      kill "$(cat "$PIDFILE")" 2>/dev/null || true
      rm -f "$PIDFILE"
    fi
    pkill -x goldeye_clysim 2>/dev/null || true
    echo "killed"
    ;;
  help|*)
    cat <<EOF
007-goldeye-clysim — voxel GoldenEye split DM

  sh button.sh compile
  sh button.sh run
  sh button.sh kill

Menu: arrows select  left/right players 2-4  type seed digits  Enter start
Play: ARROWS move/strafe  A/D turn  J jump  F fire  E vehicle  P pause
  Space sprint  1=1st/2=3rd cam  S reset cam  Esc menu  Ctrl+C quit
Top-left = human; other panes = AI. Mini-map per pane. Frag limit 10. K/D.
EOF
    ;;
esac
