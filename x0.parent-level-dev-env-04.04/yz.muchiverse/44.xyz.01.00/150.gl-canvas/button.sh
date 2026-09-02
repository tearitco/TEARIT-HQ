#!/bin/bash
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -Wextra -O2"
SYS="$DIR/system"
TH="$DIR/test-harn"

case "${1:-compile}" in
  compile|c|build)
    echo "=== compiling gl-canvas ==="
    $CC $CFLAGS -o "$SYS/gl_canvas" "$SYS/gl_canvas.c" -lglut -lGL -lGLU -lX11
    echo "  gl_canvas OK"
    $CC $CFLAGS -o "$SYS/import_pet" "$SYS/import_pet.c"
    echo "  import_pet OK"
    $CC $CFLAGS -o "$SYS/pet_purely" "$SYS/pet_purely.c" -lX11 -lXext -lGL -lm
    echo "  pet_purely OK"
    echo "=== compiling test-harn ==="
    $CC $CFLAGS -o "$TH/ops/+x/tk_drag_sim.+x" "$TH/ops/tk_drag_sim.c" -lX11
    echo "  tk_drag_sim OK"
    $CC $CFLAGS -o "$TH/ops/+x/tk_screenshot.+x" "$TH/ops/tk_screenshot.c" -lX11
    echo "  tk_screenshot OK"
    echo "=== all compiled ==="
    ;;
  run|r|start)
    cd "$DIR" && exec "$SYS/gl_canvas"
    ;;
  run-purely|purely|pet)
    cd "$DIR" && exec "$SYS/pet_purely" "${@:2}"
    ;;
  kill|k|stop)
    pkill -f gl_canvas 2>/dev/null || true
    pkill -f pet_purely 2>/dev/null || true
    pkill -f import_pet 2>/dev/null || true
    echo "killed"
    ;;
  test|t)
    bash "$TH/scenarios/test_drag_drop.sh"
    ;;
  *)
    echo "Usage: $0 {compile|run|run-purely|kill|test}"
    exit 1
    ;;
esac
