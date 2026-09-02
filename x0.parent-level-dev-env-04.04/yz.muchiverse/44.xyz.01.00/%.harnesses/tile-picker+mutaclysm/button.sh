#!/bin/bash
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../.." && pwd)"
ACTION="${1:-help}"
MUTA="$(ls -d "$HOUSE"/101.mutaclsym* 2>/dev/null | head -1)"
FMENU="$(find "$HOUSE" -maxdepth 2 -type d -name 'file-menu' 2>/dev/null | head -1)"
TP="$(find "$HOUSE" -maxdepth 2 -type d -name 'tile-picker' 2>/dev/null | head -1)"
case "$ACTION" in
    compile|c|build)
        mkdir -p "$MUTA/ops/+x"
        for o in muta_world_io muta_widget_cmds muta_map_io muta_place_tile; do
            gcc -Wall -Wextra -O2 -o "$MUTA/ops/+x/$o.+x" "$MUTA/ops/$o.c" && echo OK $o
        done
        bash "$FMENU/button.sh" compile
        bash "$TP/button.sh" compile
        echo compile ok
        ;;
    demo)
        bash "$0" compile || exit 1
        bash "$SCRIPT_DIR/scenarios/demo_place_tile.sh"
        ;;
    help|*) echo "tile-picker+mutaclysm: compile | demo" ;;
esac
