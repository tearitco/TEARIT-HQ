#!/bin/bash
# build.sh for gem-dev project

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

# Ensure directories exist
mkdir -p manager/+x ops/+x

echo "--- Building Groq-Ollama Manager ---"
gcc -Wall -Wextra -O2 manager/gem-dev_manager.c -o manager/+x/gem-dev_manager.+x

echo "--- Building Groq-Ollama Bridge ---"
gcc -Wall -Wextra -O2 ops/src/gem-dev.c -o ops/+x/gem-dev.+x

echo "--- Building Groq-Ollama Agent Tools ---"
for tool_src in ops/src/*.c; do
    tool_name=$(basename "$tool_src" .c)
    # Explicitly compile bridge
    if [ "$tool_name" == "gem-dev" ]; then 
        gcc -Wall -Wextra -O2 ops/src/gem-dev.c -o "ops/+x/gem-dev"
        continue;
    fi
    
    echo "  Compiling $tool_name..."
    gcc -Wall -Wextra -O2 "$tool_src" -o "ops/+x/$tool_name"
done

echo "--- Build Complete ---"
ls -l manager/+x/gem-dev_manager.+x ops/+x/
