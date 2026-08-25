#!/bin/bash
# xo-pet-launch.sh - Canonical XO-PET launcher.

set -e

ACTION="${1:-help}"
shift || true

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/projects/xo-pet-v1"

case "$ACTION" in
    auto)
        exec "$SCRIPT_DIR/../pieces/world_tank_01/+x/manager.+x" "$@"
        ;;
    controller|r)
        exec bash "$SCRIPT_DIR/pieces/buttons/shared/run_orchestrator.sh" "$@"
        ;;
    init)
        exec bash "$SCRIPT_DIR/xo-pet-init.sh" "$@"
        ;;
    build|build-ops)
        cd "$SCRIPT_DIR"
        mkdir -p "projects/xo-pet-v1/pieces/manager/+x"
        gcc -o "projects/xo-pet-v1/pieces/manager/+x/xo-pet-v1_manager.+x" \
            "projects/xo-pet-v1/pieces/manager/xo-pet_manager.c" \
            -pthread -Wno-format-truncation -Wno-format-extra-args
        exec bash "./#.dev-storage/#.tools/compile_all.sh" "$@"
        ;;
    setup|setup-tank)
        exec bash "$SCRIPT_DIR/xo-pet-setup-tank.sh" "$@"
        ;;
    help|h|-h|--help)
        cat <<'EOF'
XO-PET launcher

Usage:
  ./xo-pet-launch.sh c
  ./xo-pet-launch.sh auto
  ./xo-pet-launch.sh controller
  ./xo-pet-launch.sh r
  ./xo-pet-launch.sh init
  ./xo-pet-launch.sh build
  ./xo-pet-launch.sh setup [pet_names...]
  ./xo-pet-launch.sh c

Compatibility wrappers still exist:
  ../button.sh
  ./button.sh
  ./build_ops.sh
  ./liz-init.sh
  ./setup_tank.sh
  ./xo-pet-run-controller.sh

Modes:
  c           Compile the active bundle.
  auto        Run the autonomous moke-pet manager path.
  controller  Run the local CHTPM orchestrator / project loader surface.
  r           Compatibility alias for controller mode.
EOF
        ;;
    *)
        echo "Unknown action: $ACTION"
        echo "Run './xo-pet-launch.sh help' for usage."
        exit 1
        ;;
esac
