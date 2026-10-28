#!/bin/sh
# build_tree_lint.sh - builds tree_lint into +x/ (one linter for evolution / tech / part trees).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/tree_lint.+x tree_lint.c
echo "OK +x/tree_lint.+x"
