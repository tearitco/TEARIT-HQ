#!/bin/sh
# build_elements_palette_manager.sh - <module> behind palettes-elements.xhtpm
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/elements_palette_manager.+x" "$HERE/elements_palette_manager.c" -lm
echo "OK $HERE/+x/elements_palette_manager.+x"
