#!/bin/bash
# run.sh for gem-api project
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$PROJECT_DIR/../.." && pwd)"
cd "$PROJECT_DIR"

mkdir -p manager state
mkdir -p pieces/display pieces/chtpm/frame_buffer

if [ ! -f "manager/+x/gem-xo_manager.+x" ]; then
    echo "Building..."
    bash build.sh || exit 1
fi

./ops/+x/startup_reset_op

echo "=== Launching Groq-Ollama Project (Foreground) ==="
echo "Project Root: $PROJECT_ROOT"

./manager/+x/gem-xo_manager.+x "$PROJECT_ROOT"
