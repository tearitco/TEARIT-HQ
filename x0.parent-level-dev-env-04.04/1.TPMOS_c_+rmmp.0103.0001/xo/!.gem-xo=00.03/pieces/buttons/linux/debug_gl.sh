#!/bin/bash
# debug_gl.sh - GL-OS Debug button for Linux/macOS
# Usage: ./pieces/buttons/linux/debug_gl.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
GL_DESKTOP="$PROJECT_ROOT/pieces/apps/gl_os/plugins/+x/gl_desktop.+x"

echo "=== Launching GL-OS Desktop (Linux) ==="
echo "Project root: $PROJECT_ROOT"

if [ ! -f "$GL_DESKTOP" ]; then
    echo "ERROR: GL-OS desktop binary not found!"
    echo "  Expected: $GL_DESKTOP"
    echo ""
    echo "Run: ./button.sh compile"
    exit 1
fi

cd "$PROJECT_ROOT"
exec "$GL_DESKTOP"
