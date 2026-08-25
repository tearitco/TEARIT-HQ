#!/bin/bash
# run.sh for gem-api project
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$PROJECT_DIR/../.." && pwd)"
cd "$PROJECT_DIR"

mkdir -p manager state
: > manager/gui_state.txt
: > state/context.json
: > state/prompt.json
: > state/llm_response.json
: > state/last_response.txt
: > state/curl_debug.log
: > state/args.tmp
rm -f state/function_call.tmp

if [ ! -f "manager/+x/gem-xo_manager.+x" ]; then
    echo "Building..."
    bash build.sh || exit 1
fi

echo "=== Launching Groq-Ollama Project (Foreground) ==="
echo "Project Root: $PROJECT_ROOT"

./manager/+x/gem-xo_manager.+x "$PROJECT_ROOT"
