#!/bin/sh
# build_frame_stats.sh - builds frame_stats into +x/ (libpng).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/frame_stats.+x frame_stats.c -lpng -lm
echo "OK +x/frame_stats.+x"
