#!/bin/bash
# run.sh for cpp-llm project
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$PROJECT_DIR/../.." && pwd)"
cd "$PROJECT_DIR"

if [ ! -f "manager/+x/cpp-llm_manager.+x" ]; then
    echo "Building..."
    bash build.sh || exit 1
fi

echo "=== Launching cpp-llm Project (Foreground) ==="
echo "Project Root: $PROJECT_ROOT"

./manager/+x/cpp-llm_manager.+x "$PROJECT_ROOT"
