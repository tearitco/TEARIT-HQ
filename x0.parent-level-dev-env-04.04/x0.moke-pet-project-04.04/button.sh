#!/bin/bash
# Top-level moke-pet / XO-PET launcher entrypoint.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-help}"

resolve_active_bundle() {
    local found
    found=$(find "$SCRIPT_DIR" -maxdepth 1 -mindepth 1 -type d -name 'x0.5-liz.fiter4-mew-*' | sort | tail -n 1)
    if [ -z "$found" ]; then
        echo "ERROR: no x0.5-liz.fiter4-mew-* bundle found under $SCRIPT_DIR" >&2
        exit 1
    fi
    printf '%s\n' "$found"
}

BUNDLE_DIR="$(resolve_active_bundle)"
BUNDLE_LAUNCHER="$BUNDLE_DIR/xo-pet-launch.sh"

if [ ! -f "$BUNDLE_LAUNCHER" ]; then
    echo "ERROR: bundle launcher missing: $BUNDLE_LAUNCHER" >&2
    exit 1
fi

case "$ACTION" in
    compile|c)
        shift || true
        echo "Compiling autonomous world manager..."
        mkdir -p "$SCRIPT_DIR/pieces/world_tank_01/+x"
        gcc -o "$SCRIPT_DIR/pieces/world_tank_01/+x/manager.+x" \
            "$SCRIPT_DIR/pieces/world_tank_01/manager.c" \
            -pthread -Wno-format-truncation -Wno-format-extra-args
        echo "✓ world manager rebuilt at pieces/world_tank_01/+x/manager.+x"
        exec bash "$BUNDLE_LAUNCHER" build "$@"
        ;;
    auto)
        shift || true
        exec "$SCRIPT_DIR/pieces/world_tank_01/+x/manager.+x" "$@"
        ;;
    controller|r)
        shift || true
        exec bash "$BUNDLE_LAUNCHER" controller "$@"
        ;;
    help|h|-h|--help)
        cat <<'EOF'
Moke-Pet / XO-PET launcher

Usage:
  sh button.sh c
  sh button.sh auto
  sh button.sh controller
  sh button.sh r
  sh button.sh init
  sh button.sh build
  sh button.sh setup [pet_names...]

Modes:
  c           Compile the world manager and the active bundle.
  auto        Run the autonomous moke-pet epoch manager from pieces/.
  controller  Run the local CHTPM orchestrator / project loader surface.
  r           Compatibility alias for controller mode.
EOF
        ;;
    *)
        exec bash "$BUNDLE_LAUNCHER" "$ACTION" "$@"
        ;;
esac
