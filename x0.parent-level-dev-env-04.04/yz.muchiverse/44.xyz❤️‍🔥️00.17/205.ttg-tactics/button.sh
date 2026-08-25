#!/bin/sh
# 205.ttg-tactics — POSIX house dual-render launcher
#   sh button.sh compile | run | kill | harness | help
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DIR" || exit 1

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--std=c11 -Wall -Wextra -O2 -I./src}
SRC="src/ttg_core.c src/ttg_compose.c src/ttg_input.c src/ttg_loop.c"
BIN="./ops/+x/ttg_loop"
PIDDIR="./.pids"
NO_GL=${NO_GL:-0}

compile() {
    echo "=== ttg-tactics: compile ==="
    mkdir -p ops/+x pieces/display pieces/apps/player_app pieces/system data
    # shellcheck disable=SC2086
    $CC $CFLAGS -o "$BIN" $SRC -lm
    echo "OK $BIN"
    if [ -x scripts/vendor_system.sh ]; then
        sh scripts/vendor_system.sh || true
    fi
}

kill_all() {
    echo "=== ttg-tactics: kill ==="
    if [ -d "$PIDDIR" ]; then
        for f in "$PIDDIR"/*; do
            [ -f "$f" ] || continue
            pid=$(cat "$f" 2>/dev/null || true)
            if [ -n "$pid" ]; then
                kill "$pid" 2>/dev/null || true
                kill -9 "$pid" 2>/dev/null || true
            fi
            rm -f "$f"
        done
    fi
    pkill -x ttg_loop 2>/dev/null || true
    # only kill mirrors we started if named under this dir — best-effort exact
    pkill -x gl_mirror 2>/dev/null || true
    pkill -x renderer 2>/dev/null || true
    pkill -x keyboard_input 2>/dev/null || true
    pkill -x chtpm_rgb_render 2>/dev/null || true
    echo "done"
}

cmd=${1:-help}
case "$cmd" in
  compile|c|build) compile ;;
  kill|k|stop) kill_all ;;
  harness|test)
    compile
    sh scripts/harness_01_move.sh
    sh scripts/harness_02_illegal_move.sh
    sh scripts/harness_03_attack_regicide.sh
    echo "=== harness all done ==="
    ;;
  run|r|start)
    compile
    kill_all 2>/dev/null || true
    mkdir -p "$PIDDIR" pieces/display pieces/apps/player_app pieces/system data
    : > pieces/system/quit_flag.txt
    : > pieces/apps/player_app/history.txt
    : > pieces/display/renderer_pulse.txt
    : > pieces/display/rgb_frame_changed.txt

    # game brain
    "$BIN" --root "$DIR" &
    echo $! > "$PIDDIR/ttg_loop.pid"
    echo "ttg_loop pid $(cat "$PIDDIR/ttg_loop.pid")"

    # dual view consumers (vendored from muta when available)
    if [ -x system/renderer ]; then
        # renderer expects cwd with pieces/display — muta-compatible paths
        (cd "$DIR" && ./system/renderer) &
        echo $! > "$PIDDIR/renderer.pid"
        echo "renderer pid $(cat "$PIDDIR/renderer.pid")"
    else
        echo "(no system/renderer — watch pieces/display/current_frame.txt)"
    fi
    if [ "$NO_GL" != "1" ] && [ -n "${DISPLAY:-}" ] && [ -x system/gl_mirror ]; then
        (cd "$DIR" && ./system/gl_mirror) &
        echo $! > "$PIDDIR/gl_mirror.pid"
        echo "gl_mirror pid $(cat "$PIDDIR/gl_mirror.pid")"
    else
        echo "(GL skip: NO_GL=$NO_GL DISPLAY=${DISPLAY:-unset} or no gl_mirror)"
    fi
    if [ -x system/keyboard_input ] && [ -t 0 ]; then
        echo "keyboard_input foreground (Ctrl+C quits)..."
        (cd "$DIR" && ./system/keyboard_input) || true
        # when keyboard exits, stop all
        kill_all
    else
        echo "No TTY keyboard_input — inject keys:"
        echo "  printf '13\\n' >> pieces/apps/player_app/history.txt   # Enter start"
        echo "  sh button.sh kill"
        if [ "${FOREGROUND:-0}" = "1" ]; then
            wait "$(cat "$PIDDIR/ttg_loop.pid")" 2>/dev/null || true
        fi
    fi
    ;;
  help|h|*)
    cat <<EOF
205.ttg-tactics — Community Tabletop Tactics (house dual-render)

  sh button.sh compile
  sh button.sh run          # loop + optional muta renderer/gl_mirror
  sh button.sh kill
  sh button.sh harness      # 01 move, 02 illegal, 03 regicide
  NO_GL=1 sh button.sh run

Keys (via history.txt decimal codes):
  Enter(13) start/select/move   arrows 1000-1003
  a(97) attack   e(101) end turn   q(113) menu/quit

See !.clone-clowning.md and DESIGN.md
EOF
    ;;
esac
