#!/bin/sh
# 201.rpg-maker-clone — POSIX launcher
#   sh button.sh compile | run | kill | help
#
ACTION="${1:-help}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BIN="$SCRIPT_DIR/rpg_clone"
PIDFILE="$SCRIPT_DIR/rpg_clone.pid"
SRC="$SCRIPT_DIR/src"

compile() {
    echo "=== compile rpg_clone (freeglut) ==="
    gcc -std=c11 -Wall -Wextra -O2 \
        -o "$BIN" \
        "$SRC/main.c" "$SRC/draw.c" "$SRC/project.c" "$SRC/tileset.c" \
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
        # optional project path: sh button.sh run projects/demo
        PROJ="${2:-projects/demo}"
        echo "run $BIN $PROJ  (DISPLAY=$DISPLAY)"
        echo "  MZ one-page Map Editor | F2 Event | F3 Play | S save | N event | q quit"
        echo "  Click tileset A-R, paint map; Play → Space on events"
        # background with pid file so kill works
        if [ "${FOREGROUND:-0}" = "1" ]; then
            exec "$BIN" "$PROJ"
        fi
        "$BIN" "$PROJ" &
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
        # also match binary name
        pkill -f "$BIN" 2>/dev/null || true
        pkill -x rpg_clone 2>/dev/null || true
        echo "kill done"
        ;;
    help|h|-h|--help|*)
        cat << EOF
201.rpg-maker-clone — self-contained RPG Maker–like editor+player

  sh button.sh compile   (c|build)   build freeglut binary
  sh button.sh run       (r|start)   compile if needed, run demo project
  sh button.sh kill      (k|stop)    stop running instance
  sh button.sh help

Env:
  FOREGROUND=1 sh button.sh run   # exec in foreground (no pid file)
  DISPLAY must be set for run

Demo:
  Title → 5 Play  (or F3)
  Walk with arrows to G (guard) or S (crystal)
  Space/Enter to run action event → message box
EOF
        ;;
esac
