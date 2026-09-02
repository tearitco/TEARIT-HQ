#!/bin/bash
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../.." && pwd)"
ACTION="${1:-help}"
MUTA="$(ls -d "$HOUSE"/101.mutaclsym* 2>/dev/null | head -1)"
FMENU="$(find "$HOUSE" -maxdepth 2 -type d -name 'file-menu' 2>/dev/null | head -1)"
MPICK="$(find "$HOUSE" -maxdepth 2 -type d -name 'map-picker' 2>/dev/null | head -1)"

case "$ACTION" in
    compile|c|build)
        mkdir -p "$MUTA/ops/+x"
        gcc -Wall -Wextra -O2 -o "$MUTA/ops/+x/muta_world_io.+x" "$MUTA/ops/muta_world_io.c" && echo OK muta_world_io
        gcc -Wall -Wextra -O2 -o "$MUTA/ops/+x/muta_widget_cmds.+x" "$MUTA/ops/muta_widget_cmds.c" && echo OK muta_widget_cmds
        gcc -Wall -Wextra -O2 -o "$MUTA/ops/+x/muta_map_io.+x" "$MUTA/ops/muta_map_io.c" && echo OK muta_map_io
        bash "$FMENU/button.sh" compile
        bash "$MPICK/button.sh" compile
        echo compile ok
        ;;
    demo)
        bash "$0" compile || exit 1
        bash "$SCRIPT_DIR/scenarios/demo_switch_map.sh"
        ;;
    help|*)
        echo "%.harnesses/map-picker+mutaclysm — compile | demo | help"
        echo "  MUTA=$MUTA MPICK=$MPICK"
        ;;
esac
