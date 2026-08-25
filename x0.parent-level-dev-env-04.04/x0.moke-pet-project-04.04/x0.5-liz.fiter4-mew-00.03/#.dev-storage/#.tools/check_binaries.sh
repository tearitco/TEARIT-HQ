#!/bin/bash
# check_binaries.sh - Verify all required binaries exist
# Usage: ./#.dev-storage/#.tools/check_binaries.sh

echo "=== BINARY VERIFICATION CHECK ==="
echo ""

MISSING=0
EXISTS=0

check_binary() {
    local path="$1"
    local name="$2"
    
    if [ -x "$path" ]; then
        echo "  ✓ $name"
        ((EXISTS++))
    else
        echo "  ✗ MISSING: $name"
        ((MISSING++))
    fi
}

echo "=== SYSTEM BINARIES ==="
check_binary "pieces/keyboard/plugins/+x/keyboard_input.+x" "keyboard_input"
check_binary "pieces/chtpm/plugins/+x/chtpm_parser.+x" "chtpm_parser"
check_binary "pieces/chtpm/plugins/+x/orchestrator.+x" "orchestrator"
check_binary "pieces/display/plugins/+x/renderer.+x" "renderer"
echo ""

echo "=== SYSTEM APP BINARIES ==="
# Note: op-ed is now a PROJECT (projects/op-ed/), checked in PROJECT MANAGERS section
# Note: man-* apps were refactored out — no longer needed
check_binary "pieces/apps/player_app/manager/plugins/+x/player_manager.+x" "player_manager"
echo ""

echo "=== PLAYRM OPS (SHARED) ==="
for op in render_map move_entity scan_op collect_op place_op inventory_op; do
    check_binary "pieces/apps/playrm/ops/+x/${op}.+x" "$op"
done
echo ""

echo "=== PROJECT MANAGERS ==="
for proj_dir in projects/*/; do
    proj_name=$(basename "$proj_dir")
    if [ "$proj_name" = "trunk" ]; then continue; fi
    
    manager_bin="${proj_dir}manager/+x/${proj_name}_manager.+x"
    if [ -f "${proj_dir}manager/${proj_name}_manager.c" ]; then
        check_binary "$manager_bin" "${proj_name}_manager"
    fi
done
echo ""

echo "=== PROJECT OPS ==="
for proj_dir in projects/*/; do
    proj_name=$(basename "$proj_dir")
    if [ "$proj_name" = "trunk" ]; then continue; fi
    
    if [ -d "${proj_dir}ops/" ]; then
        for src in "${proj_dir}ops/"*.c; do
            if [ -f "$src" ]; then
                op_name=$(basename "$src" .c)
                check_binary "${proj_dir}ops/+x/${op_name}.+x" "${proj_name}::${op_name}"
            fi
        done
    fi
done
echo ""

echo "=== SUMMARY ==="
echo "  Present:  $EXISTS binaries"
echo "  Missing:  $MISSING binaries"
echo ""

if [ $MISSING -gt 0 ]; then
    echo "⚠ ACTION REQUIRED: $MISSING binaries are missing!"
    echo ""
    echo "To compile all missing binaries:"
    echo "  ./#.dev-storage/#.tools/compile_all.sh"
    echo ""
    echo "Or compile individual binary:"
    echo "  gcc -o <path>/<name>.+x <path>/<name>.c -pthread"
    exit 1
else
    echo "✓ All required binaries present!"
    exit 0
fi
