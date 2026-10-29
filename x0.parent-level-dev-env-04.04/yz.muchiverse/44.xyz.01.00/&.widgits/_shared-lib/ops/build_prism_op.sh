#!/bin/sh
# build_prism_op.sh - builds prism_op into +x/ (renders the Custom theme prism from 4 variables).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/prism_op.+x prism_op.c -lm
echo "OK +x/prism_op.+x"
