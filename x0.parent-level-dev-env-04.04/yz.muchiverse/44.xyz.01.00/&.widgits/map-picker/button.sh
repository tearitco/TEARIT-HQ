#!/bin/bash
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
case "$ACTION" in
    compile|c|build)
        mkdir -p "$SCRIPT_DIR/ops/+x"
        for op in mp_list_maps mp_switch_map; do
            gcc -Wall -Wextra -O2 -o "$SCRIPT_DIR/ops/+x/$op.+x" "$SCRIPT_DIR/ops/$op.c" \
                && echo "OK $op" || echo "FAIL $op"
        done
        ;;
    help|*)
        echo "map-picker: compile | help"
        echo "  ops: mp_list_maps, mp_switch_map (needs mutaclysm focus)"
        ;;
esac
