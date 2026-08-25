#!/bin/bash
CC=gcc
CFLAGS="-std=c11 -Wall -Wextra -O2 -D_DEFAULT_SOURCE -pthread"
TOOL_DIR="tools"

echo "Building agent..."
$CC $CFLAGS -o agent agent.c

echo "Building tools..."
for f in $TOOL_DIR/*.c; do
    tool_name="${f%.c}"
    echo "  $tool_name"
    $CC $CFLAGS -o "$tool_name" "$f"
done
echo "Done."
