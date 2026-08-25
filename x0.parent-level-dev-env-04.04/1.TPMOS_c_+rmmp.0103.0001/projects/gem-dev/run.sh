#!/bin/bash
# run.sh for gem-dev project
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$PROJECT_DIR/../.." && pwd)"
cd "$PROJECT_DIR"

mkdir -p manager state

if [ ! -f "manager/+x/gem-dev_manager.+x" ]; then
    echo "Building..."
    bash build.sh || exit 1
fi

./ops/+x/startup_reset_op

echo "=== Launching Groq-Ollama Project (Foreground) ==="
echo "Project Root: $PROJECT_ROOT"

./manager/+x/gem-dev_manager.+x "$PROJECT_ROOT"
