#!/bin/bash
# Multi-project harness: file-menu widget × 102.editor (cmd bus).
# House law §36: cross-project harness lives under %.harnesses/
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# harness at HOUSE/%.harnesses/file-menu+editor
HOUSE="$(cd "$SCRIPT_DIR/../.." && pwd)"
ACTION="${1:-help}"

EDITOR_DIR="$(ls -d "$HOUSE"/102.editor* 2>/dev/null | head -1)"
FMENU_DIR="$(find "$HOUSE" -maxdepth 2 -type d -name 'file-menu' 2>/dev/null | head -1)"

case "$ACTION" in
    compile|c|build)
        echo "=== compile editor ==="
        bash "$EDITOR_DIR/button.sh" compile || exit 1
        echo "=== compile file-menu ops ==="
        bash "$FMENU_DIR/button.sh" compile || exit 1
        echo "=== compile harness helpers ==="
        mkdir -p "$SCRIPT_DIR/ops/+x"
        for op in hm_assert_file hm_assert_kv; do
            if [ -f "$SCRIPT_DIR/ops/$op.c" ]; then
                gcc -Wall -Wextra -O2 -o "$SCRIPT_DIR/ops/+x/$op.+x" "$SCRIPT_DIR/ops/$op.c" \
                    && echo "OK $op" || echo "FAIL $op"
            fi
        done
        echo "compile ok"
        ;;
    demo)
        bash "$0" compile || exit 1
        bash "$SCRIPT_DIR/scenarios/demo_load_save.sh"
        ;;
    kill|k|stop)
        bash "$EDITOR_DIR/button.sh" kill >/dev/null 2>&1 || true
        pkill -f "system/prisc\+x" 2>/dev/null || true
        echo "clean"
        ;;
    help|*)
        echo "%.harnesses/file-menu+editor — multi-project harness"
        echo "  HOUSE=$HOUSE"
        echo "  EDITOR=$EDITOR_DIR"
        echo "  FMENU=$FMENU_DIR"
        echo "  compile | demo | kill | help"
        ;;
esac
