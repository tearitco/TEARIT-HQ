#!/bin/sh
# build_raw_to_png.sh - builds raw_to_png into +x/ (libpng).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/raw_to_png.+x raw_to_png.c -lpng -lm
echo "OK +x/raw_to_png.+x"
