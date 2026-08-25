#!/bin/bash
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../.." && pwd)"
ACTION="${1:-help}"
PM="$HOUSE/&.widgits/proc-monitor"
case "$ACTION" in
    compile|c|build)
        bash "$PM/button.sh" compile
        ;;
    demo)
        bash "$0" compile || exit 1
        bash "$SCRIPT_DIR/scenarios/demo_register_focus_kill.sh"
        ;;
    help|*) echo "proc-monitor harness: compile | demo"; echo "PM=$PM" ;;
esac
