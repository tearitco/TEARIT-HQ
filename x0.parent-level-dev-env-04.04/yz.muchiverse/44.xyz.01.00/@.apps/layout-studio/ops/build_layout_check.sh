#!/bin/sh
# builds layout_check (layout studio layer 1+2). Usage: sh ops/build_layout_check.sh
cd "$(dirname "$0")" || exit 1
mkdir -p +x
gcc -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -o +x/layout_check.+x layout_check.c && echo "OK $(pwd)/+x/layout_check.+x"
