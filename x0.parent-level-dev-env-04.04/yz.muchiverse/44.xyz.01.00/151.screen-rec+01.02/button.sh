#!/bin/bash
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -Wextra -O2"
SYS="$DIR/system"
TH="$DIR/test-harn"

PW_FLAGS="$(pkg-config --cflags --libs libpipewire-0.3)"
GIO_FLAGS="$(pkg-config --cflags --libs gio-2.0 gio-unix-2.0)"
AV_FLAGS="$(pkg-config --cflags --libs libavcodec libavformat libavutil libswscale)"

export SCREENREC_PROJECT_ROOT="$DIR"

case "${1:-compile}" in
  deps|d|install-deps)
    echo "=== installing build deps (apt) ==="
    sudo apt install -y \
      libpipewire-0.3-dev libspa-0.2-dev libx264-dev \
      libglib2.0-dev \
      freeglut3-dev libgl-dev libglu1-mesa-dev \
      libx11-dev \
      ffmpeg \
      pkg-config gcc
    echo "=== deps installed ==="
    ;;
  compile|c|build)
    echo "=== compiling screen-rec ==="
    $CC $CFLAGS -o "$SYS/screen_rec" "$SYS/screen_rec.c" $PW_FLAGS $GIO_FLAGS $AV_FLAGS
    echo "  screen_rec OK"
    $CC $CFLAGS -o "$SYS/screen_rec_gui" "$SYS/screen_rec_gui.c" -lglut -lGL -lGLU
    echo "  screen_rec_gui OK"
    echo "=== compiling test-harn ==="
    mkdir -p "$TH/ops/+x"
    $CC $CFLAGS -o "$TH/ops/+x/tk_screenshot.+x" "$TH/ops/tk_screenshot.c" -lX11
    echo "  tk_screenshot OK"
    $CC $CFLAGS -o "$TH/ops/+x/tk_click.+x" "$TH/ops/tk_click.c" -lX11
    echo "  tk_click OK"
    echo "=== all compiled ==="
    ;;
  run|r|start)
    mkdir -p "$DIR/pieces/display" "$DIR/pieces/control" "$DIR/recordings"
    "$SYS/screen_rec" &
    echo $! > /tmp/screen_rec.pid
    sleep 1
    cd "$DIR" && exec "$SYS/screen_rec_gui"
    ;;
  kill|k|stop)
    pkill -f screen_rec_gui 2>/dev/null || true
    pkill -f "system/screen_rec\$" 2>/dev/null || true
    if [ -f /tmp/screen_rec.pid ]; then
      kill "$(cat /tmp/screen_rec.pid)" 2>/dev/null || true
      rm -f /tmp/screen_rec.pid
    fi
    echo "killed"
    ;;
  test|t)
    bash "$TH/scenarios/test_record_flow.sh"
    ;;
  *)
    echo "Usage: $0 {deps|compile|run|kill|test}"
    exit 1
    ;;
esac
