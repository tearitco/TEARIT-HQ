#!/bin/sh
# build_tomom_manager.sh - compile the tomom-hq window backend (self-contained .c).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -O2 \
    -o "$HERE/+x/tomom_manager.+x" "$HERE/tomom_manager.c" -lm
echo "OK $HERE/+x/tomom_manager.+x"
