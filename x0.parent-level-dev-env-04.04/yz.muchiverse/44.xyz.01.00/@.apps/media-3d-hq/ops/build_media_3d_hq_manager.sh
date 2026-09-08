#!/bin/sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/media_3d_hq_manager.+x" "$HERE/media_3d_hq_manager.c" -lm
echo "OK $HERE/+x/media_3d_hq_manager.+x"
