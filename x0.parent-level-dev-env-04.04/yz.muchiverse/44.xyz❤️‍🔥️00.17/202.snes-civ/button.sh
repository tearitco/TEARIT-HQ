#!/bin/sh
# 202.snes-civ — POSIX launcher
#   sh button.sh compile | run | kill | help
#
ACTION="${1:-help}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BIN="$SCRIPT_DIR/snes-civ"
PIDFILE="$SCRIPT_DIR/snes-civ.pid"
SRC="$SCRIPT_DIR/src"

compile() {
    echo "=== compile snes-civ (freeglut) ==="
    gcc -std=c11 -Wall -Wextra -O2 \
        -o "$BIN" \
        "$SRC/main.c" "$SRC/map.c" "$SRC/game.c" "$SRC/render.c" \
        -lGL -lGLU -lglut -lm
    ec=$?
    if [ "$ec" -eq 0 ]; then
        echo "OK  $BIN"
    else
        echo "FAIL compile exit=$ec"
        return "$ec"
    fi
}

case "$ACTION" in
    compile|c|build)
        compile
        ;;
    run|r|start|open)
        if [ -z "${DISPLAY:-}" ]; then
            echo "No DISPLAY — cannot open freeglut window"
            exit 1
        fi
        if [ ! -x "$BIN" ]; then
            compile || exit 1
        fi
        cd "$SCRIPT_DIR" || exit 1
        echo "run $BIN  (DISPLAY=$DISPLAY)"
        echo "  Arrows/click move | N next unit | B found city | Space end turn | Q quit"
        # drop action verb so optional --seed reaches the binary
        shift
        if [ "${FOREGROUND:-0}" = "1" ]; then
            exec "$BIN" "$@"
        fi
        "$BIN" "$@" &
        echo $! > "$PIDFILE"
        echo "pid $(cat "$PIDFILE")  (sh button.sh kill to stop)"
        wait "$(cat "$PIDFILE")" 2>/dev/null || true
        rm -f "$PIDFILE"
        ;;
    kill|k|stop)
        if [ -f "$PIDFILE" ]; then
            pid=$(cat "$PIDFILE")
            kill "$pid" 2>/dev/null || true
            sleep 0.2
            kill -9 "$pid" 2>/dev/null || true
            rm -f "$PIDFILE"
            echo "killed pid $pid"
        fi
        pkill -f "$BIN" 2>/dev/null || true
        pkill -x snes-civ 2>/dev/null || true
        rm -f "$PIDFILE"
        echo "kill done"
        ;;
    help|h|-h|--help|*)
        cat << EOF
202.snes-civ — SNES-era Civilization clone (C + freeglut)

  sh button.sh compile   (c|build)   build ./snes-civ
  sh button.sh run       (r|start)   compile if needed, run
  sh button.sh kill      (k|stop)    stop running instance
  sh button.sh help

Env:
  FOREGROUND=1 sh button.sh run   # exec in foreground
  DISPLAY must be set for run

Optional:
  sh button.sh run --seed 42

Play:
  Arrows / click   move selected unit (or attack)
  N / Tab          next unit with moves
  B                found city (Settler)
  [ ]              cycle city production
  WASD             pan camera
  Space / E        end turn (AI acts)
  Q / Esc          quit
EOF
        if [ "$ACTION" = "help" ] || [ "$ACTION" = "h" ] || \
           [ "$ACTION" = "-h" ] || [ "$ACTION" = "--help" ]; then
            exit 0
        fi
        exit 1
        ;;
esac
