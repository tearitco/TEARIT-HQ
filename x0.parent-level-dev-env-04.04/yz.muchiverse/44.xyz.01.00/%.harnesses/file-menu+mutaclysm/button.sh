#!/bin/bash
# Multi-project harness: file-menu × mutaclysm world save/load (user FS slots)
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../.." && pwd)"
ACTION="${1:-help}"

MUTA_DIR="$(ls -d "$HOUSE"/101.mutaclsym* 2>/dev/null | head -1)"
FMENU_DIR="$(find "$HOUSE" -maxdepth 2 -type d -name 'file-menu' 2>/dev/null | head -1)"

case "$ACTION" in
    compile|c|build)
        mkdir -p "$MUTA_DIR/ops/+x" "$FMENU_DIR/ops/+x"
        echo "=== mutaclysm world IO ==="
        gcc -Wall -Wextra -O2 -o "$MUTA_DIR/ops/+x/muta_world_io.+x" "$MUTA_DIR/ops/muta_world_io.c" && echo OK muta_world_io
        gcc -Wall -Wextra -O2 -o "$MUTA_DIR/ops/+x/muta_widget_cmds.+x" "$MUTA_DIR/ops/muta_widget_cmds.c" && echo OK muta_widget_cmds
        echo "=== file-menu ==="
        bash "$FMENU_DIR/button.sh" compile
        echo "compile ok"
        ;;
    demo)
        bash "$0" compile || exit 1
        bash "$SCRIPT_DIR/scenarios/demo_save_load.sh"
        ;;
    kill|k|stop)
        echo "clean (no long-lived procs in this harness)"
        ;;
    help|*)
        echo "%.harnesses/file-menu+mutaclysm"
        echo "  HOUSE=$HOUSE"
        echo "  MUTA=$MUTA_DIR"
        echo "  FMENU=$FMENU_DIR"
        echo "  compile | demo | kill | help"
        ;;
esac
