#!/bin/sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/sprite_cache_op.+x" "$HERE/sprite_cache_op.c"
echo "OK $HERE/+x/sprite_cache_op.+x"
