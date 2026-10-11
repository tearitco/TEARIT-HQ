#!/bin/sh
# build_side_toggle.sh - builds ops/+x/side_toggle.+x (run from @.apps/layout-studio)
cd "$(dirname "$0")/.." || exit 1
mkdir -p ops/+x
gcc -std=c11 -O2 -Wall -Wextra -o ops/+x/side_toggle.+x ops/side_toggle.c
