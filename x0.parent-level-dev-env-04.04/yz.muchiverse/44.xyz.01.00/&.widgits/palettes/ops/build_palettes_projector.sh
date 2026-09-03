#!/bin/sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/palettes_projector.+x" "$HERE/palettes_projector.c"
echo "OK $HERE/+x/palettes_projector.+x"
