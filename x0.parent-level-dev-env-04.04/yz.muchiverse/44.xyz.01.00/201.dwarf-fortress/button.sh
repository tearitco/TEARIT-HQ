#!/bin/sh
# button.sh — POSIX compile | run | kill | help for dwarf-fortress
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DIR" || exit 1

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--std=c11 -Wall -Wextra -O2 -I./src}
LIBS="-lGL -lGLU -lglut -lm"
BIN="./dwarf_fortress"
SRC="src/main.c src/map.c src/unit.c src/job.c src/render.c src/save.c"
PIDFILE="./.dwarf_fortress.pid"

cmd=${1:-help}

case "$cmd" in
  compile|c|build)
    echo "=== dwarf-fortress: compile ==="
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
    echo "=== dwarf-fortress: run ==="
    "$BIN" &
    echo $! > "$PIDFILE"
    echo "pid $(cat "$PIDFILE") — Space pause; d dig; t cut; b build; Shift+Q quit"
    wait "$(cat "$PIDFILE")" 2>/dev/null || true
    rm -f "$PIDFILE"
    ;;
  kill|k|stop)
    echo "=== dwarf-fortress: kill ==="
    if [ -f "$PIDFILE" ]; then
      pid=$(cat "$PIDFILE")
      kill "$pid" 2>/dev/null || true
      rm -f "$PIDFILE"
      echo "killed pid $pid"
    fi
    pkill -f "$DIR/dwarf_fortress" 2>/dev/null || true
    pkill -x dwarf_fortress 2>/dev/null || true
    echo "done"
    ;;
  help|h|-h|--help|*)
    cat <<EOF
dwarf-fortress button.sh (POSIX sh)

  sh button.sh compile   # build ./dwarf_fortress
  sh button.sh run       # compile if needed, run fort
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
