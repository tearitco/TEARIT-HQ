#!/bin/bash
# run.sh for groq-ollama project
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$PROJECT_DIR/../.." && pwd)"
cd "$PROJECT_DIR"

if [ ! -f "manager/+x/groq-ollama_manager.+x" ]; then
    echo "Building..."
    bash build.sh || exit 1
fi

echo "=== Launching Groq-Ollama Project (Foreground) ==="
echo "Project Root: $PROJECT_ROOT"

./manager/+x/groq-ollama_manager.+x "$PROJECT_ROOT"
