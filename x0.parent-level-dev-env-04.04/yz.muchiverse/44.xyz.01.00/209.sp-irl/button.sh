#!/bin/sh
# SP-IRL — POSIX launcher
#   sh button.sh compile | run | kill | help
#
ACTION="${1:-help}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BIN="$SCRIPT_DIR/spirl"
PIDFILE="$SCRIPT_DIR/.spirl.pid"
SRC="$SCRIPT_DIR/src"

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--std=c11 -Wall -Wextra -O2 -I"$SRC"}
LIBS="-lGL -lGLU -lglut -lm"
SRCS="$SRC/main.c $SRC/map.c $SRC/mon.c $SRC/item.c $SRC/render.c $SRC/save.c $SRC/tactics.c"

compile() {
    echo "=== compile spirl (freeglut) ==="
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
        echo "  Title → New Game / PvP Battle"
        echo "  Overworld: arrows walk, M=party, B=bag, F=fly, P=tile=heal, G=gym"
        echo "  Tall grass & trainers → tactics battle (Z select X skip 1/2 moves Q force-end)"
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
        pkill -9 -f spirl 2>/dev/null || true
        pkill -9 -x spirl 2>/dev/null || true
        killall -9 spirl 2>/dev/null || true
        echo "kill done"
        ;;
    help|h|-h|--help|*)
        cat << EOF
SP-IRL — tactical monsters (C + freeglut)

  sh button.sh compile   (c|build)   build ./spirl
  sh button.sh run       (r|start)   compile if needed, run (cwd = package)
  sh button.sh kill      (k|stop)    stop running instance
  sh button.sh help

Env:
  FOREGROUND=1 sh button.sh run   # exec in foreground
  DISPLAY must be set for run
  CC CFLAGS override toolchain

How to play (MVP):
  1. Title → NEW GAME / CONTINUE / PVP BATTLE / QUIT
   2. New: pick starter (LEAFY / EMBER / BUBBLE) → Pallet Town
   3. Walk (arrows/WASD). Tall grass → tactics battle
   4. Tactics: Z=select, X=skip, arrows=move, 1/2=pick moves, Q=force-end turn
   5. M=party menu  B=bag (potions)  P tile=heal  G tile=gym battle
   6. Walk south out of Pallet → Route 1 → Viridian City (Brock gym)
   7. F=fly menu (fast travel between connected towns)
   8. F5 save → saves/slot0/

Deps: gcc, freeglut (-lglut -lGL -lGLU), libm
EOF
        if [ "$ACTION" = "help" ] || [ "$ACTION" = "h" ] || \
           [ "$ACTION" = "-h" ] || [ "$ACTION" = "--help" ]; then
            exit 0
        fi
        exit 1
        ;;
esac
