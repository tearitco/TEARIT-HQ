#!/bin/sh
# build_knowledge_manager.sh - compile the knowledge-hq window backend (self-contained .c) into ops/+x/ (git-ignored).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-format-truncation -O2 \
    -o "$HERE/+x/knowledge_manager.+x" "$HERE/knowledge_manager.c" -lm
echo "OK $HERE/+x/knowledge_manager.+x"
