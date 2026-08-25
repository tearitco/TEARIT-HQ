#!/bin/bash
# Compatibility wrapper from the old generic launcher name.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
case "${1:-help}" in
    compile|c)
        shift || true
        exec bash "$SCRIPT_DIR/xo-pet-launch.sh" build "$@"
        ;;
    *)
        exec bash "$SCRIPT_DIR/xo-pet-launch.sh" "$@"
        ;;
esac
