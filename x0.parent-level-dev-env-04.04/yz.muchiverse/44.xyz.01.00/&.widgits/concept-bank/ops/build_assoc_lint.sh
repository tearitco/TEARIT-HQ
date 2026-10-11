#!/bin/sh
# build_assoc_lint.sh - compile assoc_lint (self-contained C).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -O2 -o +x/assoc_lint.+x assoc_lint.c
echo "OK +x/assoc_lint.+x"
