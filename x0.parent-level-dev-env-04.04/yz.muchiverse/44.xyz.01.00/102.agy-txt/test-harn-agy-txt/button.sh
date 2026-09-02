#!/bin/bash
# button.sh - test-harn-agy-txt launcher.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-help}"

case "$ACTION" in
    demo|save_load)
        bash "$SCRIPT_DIR/scenarios/demo_save_load.sh"
        ;;
    help|*)
        echo "test-harn-agy-txt button.sh"
        echo "  demo, save_load  - real edit/save/new/load loop, real key injection"
        ;;
esac
