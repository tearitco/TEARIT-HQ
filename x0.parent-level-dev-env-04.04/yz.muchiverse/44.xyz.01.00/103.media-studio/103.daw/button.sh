#!/bin/bash
# button.sh — Muchi DAW (103.media-studio/103.daw)
# Phase-1 foundation: freeglut UI + PulseAudio soft-synth.
# Full CHTPM shell is Phase 2 (see docs/MVP_NOTE.txt, house-design-docs.md).
#
#   sh button.sh compile
#   sh button.sh r          # run DAW window
#   sh button.sh kill
#   sh button.sh check
set -e
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$SCRIPT_DIR/ops/+x/daw_main.+x"
PIDFILE="$SCRIPT_DIR/pieces/display/daw.pid"

compile() {
    mkdir -p "$SCRIPT_DIR/ops/+x" \
             "$SCRIPT_DIR/pieces/apps/player_app" \
             "$SCRIPT_DIR/pieces/display"
    gcc -std=c11 -Wall -O2 -I"$SCRIPT_DIR/../shared" -o "$BIN" \
        "$SCRIPT_DIR/ops/daw_main.c" \
        "$SCRIPT_DIR/../shared/chtpm_nav_mock.c" \
        -lglut -lGL -lGLU -lX11 -lpulse -lpulse-simple -lpthread -lm
    echo "OK $BIN"
}

case "$ACTION" in
    compile|c|build)
        compile
        ;;
    run|r|start|widget)
        compile
        # single instance
        if [ -f "$PIDFILE" ]; then
            old=$(cat "$PIDFILE" 2>/dev/null || true)
            if [ -n "$old" ] && kill -0 "$old" 2>/dev/null; then
                echo "DAW already running pid=$old  (button.sh kill first)"
                exit 1
            fi
        fi
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        : > "$SCRIPT_DIR/pieces/apps/player_app/interact_relay.txt"
        setsid "$BIN" "$SCRIPT_DIR" >/tmp/muchi-daw.log 2>&1 < /dev/null &
        echo $! > "$PIDFILE"
        echo "Muchi DAW started pid=$(cat "$PIDFILE")"
        echo "  log: /tmp/muchi-daw.log"
        echo "  keys: Space play | A-L type | 1-8 track | R rec | s save | Esc quit"
        echo "  demo sequence loads if sequence.txt empty"
        ;;
    kill|k|stop)
        if [ -f "$PIDFILE" ]; then
            kill "$(cat "$PIDFILE")" 2>/dev/null || true
            rm -f "$PIDFILE"
        fi
        # exact basename only
        pkill -x "daw_main.+x" 2>/dev/null || true
        echo "DAW stopped"
        ;;
    check)
        [ -x "$BIN" ] && echo "OK binary" || echo "MISSING binary — run compile"
        [ -n "${DISPLAY:-}" ] && echo "OK DISPLAY=$DISPLAY" || echo "WARN no DISPLAY"
        pkg-config --exists libpulse-simple && echo "OK pulse" || echo "WARN pulse"
        ;;
    help|h|-h|--help|*)
        cat <<EOF
Muchi DAW — media-studio Phase 1 (look/feel MVP)

  sh button.sh compile   build ops/+x/daw_main.+x
  sh button.sh r         compile + launch window
  sh button.sh kill      stop
  sh button.sh check

Features (MVP): multi-track piano roll, playhead, transport, musical
typing, record-to-sequence, mute/solo/arm, EQ/Reverb/Distortion inserts
(audio path), save sequence.txt + daw_state.txt, canvas.raw export.

Not yet: real VST, wav import, CHTPM chrome shell, multi-out.
EOF
        ;;
esac
