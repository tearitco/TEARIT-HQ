#!/bin/bash
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
case "$ACTION" in
    compile|c|build)
        mkdir -p "$SCRIPT_DIR/ops/+x"
        for op in runtime_register runtime_list runtime_focus runtime_kill; do
            gcc -Wall -Wextra -O2 -o "$SCRIPT_DIR/ops/+x/$op.+x" "$SCRIPT_DIR/ops/$op.c" && echo OK $op
        done
        ;;
    help|*)
        echo "proc-monitor: compile | help"
        echo "  runtime_register | runtime_list [--gc] | runtime_focus | runtime_kill"
        ;;
esac
