#!/bin/bash
# compile.sh - Compile button for Linux/macOS
# Usage: ./pieces/buttons/linux/compile.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/../../.."
FONDU="$PROJECT_ROOT/pieces/system/fondu/fondu.+x"

echo "=== Compiling CHTPM (Linux) ==="
echo "Killing existing processes first..."
bash "$SCRIPT_DIR/kill.sh"

echo ""
echo "Compiling projects via Fondu..."

# Compile all active projects using Fondu
cd "$PROJECT_ROOT"

# Read compiled_projects.txt and compile each
if [ -f "pieces/os/compiled_projects.txt" ]; then
    while IFS= read -r line; do
        # Skip comments and empty lines
        [[ "$line" =~ ^#.*$ ]] && continue
        [[ -z "$line" ]] && continue
        
        # Trim whitespace
        project=$(echo "$line" | xargs)
        [[ -z "$project" ]] && continue
        
        echo "  Compiling: $project"
        # Compile project binaries if they have a manager or ops
        if [ -d "projects/$project/manager" ]; then
            gcc -o "projects/$project/manager/+x/"*_manager.+x "projects/$project/manager/"*_manager.c -pthread 2>/dev/null && \
                echo "    ✓ manager compiled" || echo "    - no manager source"
        fi
        if [ -d "projects/$project/ops" ]; then
            for op in projects/$project/ops/*.c; do
                [ -f "$op" ] || continue
                basename=$(basename "$op" .c)
                gcc -o "projects/$project/ops/+x/$basename.+x" "$op" 2>/dev/null && \
                    echo "    ✓ $basename compiled"
            done
        fi
    done < "pieces/os/compiled_projects.txt"
fi

# Also compile system apps (op-ed, man-*, etc.)
echo ""
echo "Compiling system apps..."

# op-ed
if [ -d "pieces/apps/op-ed/plugins" ]; then
    gcc -o pieces/apps/op-ed/plugins/+x/op-ed_module.+x pieces/apps/op-ed/plugins/op-ed_module.c -pthread 2>/dev/null && echo "  ✓ op-ed_module"
fi

# man-ops, man-pal, man-add
for app in man-ops man-pal man-add; do
    if [ -d "pieces/apps/$app/plugins" ]; then
        gcc -o "pieces/apps/$app/plugins/+x/${app}_module.+x" "pieces/apps/$app/plugins/${app}_module.c" -pthread 2>/dev/null && echo "  ✓ ${app}_module"
    fi
done

# System components
echo ""
echo "Compiling system components..."

# Keyboard input
gcc -D_GNU_SOURCE -o pieces/keyboard/plugins/+x/keyboard_input.+x pieces/keyboard/src/keyboard_input_linux.c 2>/dev/null && echo "  ✓ keyboard_input"

# CHTPM parser
gcc -o pieces/chtpm/plugins/+x/chtpm_parser.+x pieces/chtpm/plugins/chtpm_parser.c 2>/dev/null && echo "  ✓ chtpm_parser"

# Orchestrator
gcc -o pieces/chtpm/plugins/+x/orchestrator.+x pieces/chtpm/plugins/orchestrator.c -pthread 2>/dev/null && echo "  ✓ orchestrator"

# Renderer
gcc -o pieces/display/plugins/+x/renderer.+x pieces/display/renderer.c 2>/dev/null && echo "  ✓ renderer"

echo ""
echo "=== Compile Complete ==="
