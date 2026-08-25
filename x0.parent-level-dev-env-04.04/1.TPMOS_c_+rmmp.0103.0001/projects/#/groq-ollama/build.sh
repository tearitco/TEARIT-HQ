#!/bin/bash
# build.sh for groq-ollama project

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

# Ensure directories exist
mkdir -p manager/+x ops/+x

echo "--- Building Groq-Ollama Manager ---"
gcc -Wall -Wextra -O2 manager/groq-ollama_manager.c -o manager/+x/groq-ollama_manager.+x

echo "--- Building Groq-Ollama Bridge ---"
gcc -Wall -Wextra -O2 ops/src/groq-ollama_bridge.c -o ops/+x/groq-ollama_bridge.+x

echo "--- Building Groq-Ollama Agent Tools ---"
for tool_src in ops/src/*.c; do
    tool_name=$(basename "$tool_src" .c)
    # Skip bridge since we already built it with a specific name above
    if [ "$tool_name" == "groq-ollama_bridge" ]; then continue; fi
    
    echo "  Compiling $tool_name..."
    gcc -Wall -Wextra -O2 "$tool_src" -o "ops/+x/$tool_name"
done

echo "--- Build Complete ---"
ls -l manager/+x/groq-ollama_manager.+x ops/+x/
