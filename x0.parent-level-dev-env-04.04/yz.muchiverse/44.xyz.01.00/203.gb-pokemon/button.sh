#!/bin/sh
# 203.gb-pokemon — POSIX launcher
#   sh button.sh compile | run | kill | help
#
ACTION="${1:-help}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BIN="$SCRIPT_DIR/gb-pokemon"
PIDFILE="$SCRIPT_DIR/.gb-pokemon.pid"
SRC="$SCRIPT_DIR/src"

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--std=c11 -Wall -Wextra -O2 -I"$SRC"}
LIBS="-lGL -lGLU -lglut -lm"
SRCS="$SRC/main.c $SRC/map.c $SRC/mon.c $SRC/battle.c $SRC/render.c $SRC/save.c"

compile() {
    echo "=== compile gb-pokemon (freeglut) ==="
    # shellcheck disable=SC2086
    $CC $CFLAGS -o "$BIN" $SRCS $LIBS
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
        echo "  Title → New Game → pick starter → walk into tall grass (dark green)"
        echo "  Battle: Fight/Run (arrows+Z) or keys 1/2 for moves  F5=save  q=quit"
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
        if [ -f "$PIDFILE" ]; then
            pid=$(cat "$PIDFILE")
            kill "$pid" 2>/dev/null || true
            sleep 0.2
            kill -9 "$pid" 2>/dev/null || true
            rm -f "$PIDFILE"
            echo "killed pid $pid"
        fi
        pkill -f "$BIN" 2>/dev/null || true
        pkill -x gb-pokemon 2>/dev/null || true
        echo "kill done"
        ;;
    help|h|-h|--help|*)
        cat << EOF
203.gb-pokemon — Game Boy Color (GBC) Pokémon MVP (C + freeglut)

  sh button.sh compile   (c|build)   build ./gb-pokemon
  sh button.sh run       (r|start)   compile if needed, run (cwd = package)
  sh button.sh kill      (k|stop)    stop running instance
  sh button.sh help

Env:
  FOREGROUND=1 sh button.sh run   # exec in foreground
  DISPLAY must be set for run
  CC CFLAGS override toolchain

How to play (MVP):
  1. Title → NEW GAME (arrows + Z/Enter)
  2. Pick starter: LEAFY / EMBER / BUBBLE
  3. Walk (arrows/WASD). Vivid green tall grass → wild battle (full color)
  4. Battle: FIGHT or RUN; keys 1/2 pick moves
  5. Red tile (P) = Pokecenter heal
  6. F5 save → saves/slot0/   Title CONTINUE loads it

Deps: gcc, freeglut (-lglut -lGL -lGLU), libm
EOF
        if [ "$ACTION" = "help" ] || [ "$ACTION" = "h" ] || \
           [ "$ACTION" = "-h" ] || [ "$ACTION" = "--help" ]; then
            exit 0
        fi
        exit 1
        ;;
esac
