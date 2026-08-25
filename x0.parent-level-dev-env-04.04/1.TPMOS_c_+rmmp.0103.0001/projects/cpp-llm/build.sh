#!/bin/bash
# build.sh for cpp-llm project

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

# Ensure directories exist
mkdir -p manager/+x ops/+x

echo "--- Building cpp-llm Manager ---"
gcc -Wall -Wextra -O2 manager/cpp-llm_manager.c -o manager/+x/cpp-llm_manager.+x

echo "--- Building cpp-llm Bridge ---"
gcc -Wall -Wextra -O2 ops/src/cpp-llm_bridge.c -o ops/+x/cpp-llm_bridge.+x

echo "--- Building cpp-llm Agent Tools ---"
for tool_src in ops/src/*.c; do
    tool_name=$(basename "$tool_src" .c)
    # Skip bridge since we already built it with a specific name above
    if [ "$tool_name" == "cpp-llm_bridge" ]; then continue; fi
    
    echo "  Compiling $tool_name..."
    gcc -Wall -Wextra -O2 "$tool_src" -o "ops/+x/$tool_name"
done

echo "--- Build Complete ---"
ls -l manager/+x/cpp-llm_manager.+x ops/+x/
