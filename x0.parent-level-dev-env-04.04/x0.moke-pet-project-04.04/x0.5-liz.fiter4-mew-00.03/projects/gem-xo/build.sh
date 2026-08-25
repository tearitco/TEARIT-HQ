#!/bin/bash
# build.sh for gem-api project

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

# Ensure directories exist
mkdir -p manager/+x ops/+x

echo "--- Building Groq-Ollama Manager ---"
gcc -Wall -Wextra -O2 manager/gem-xo_manager.c -o manager/+x/gem-xo_manager.+x

echo "--- Building Groq-Ollama Bridge ---"
gcc -Wall -Wextra -O2 ops/src/gem-xo.c -o ops/+x/gem-xo.+x

echo "--- Building Groq-Ollama Agent Tools ---"
for tool_src in ops/src/*.c; do
    tool_name=$(basename "$tool_src" .c)
    # Explicitly compile bridge
    if [ "$tool_name" == "gem-xo" ]; then 
        gcc -Wall -Wextra -O2 ops/src/gem-xo.c -o "ops/+x/gem-xo"
        continue;
    fi
    
    echo "  Compiling $tool_name..."
    gcc -Wall -Wextra -O2 "$tool_src" -o "ops/+x/$tool_name"
done

echo "--- Build Complete ---"
ls -l manager/+x/gem-xo_manager.+x ops/+x/
