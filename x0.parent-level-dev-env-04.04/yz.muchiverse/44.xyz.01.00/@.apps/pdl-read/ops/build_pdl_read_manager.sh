#!/bin/sh
# build_pdl_read_manager.sh - compile pdl-read's own document/pagination
# manager. Mirrors file-explorer/ops/build_file_explorer_manager.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/pdl_read_manager.+x" "$HERE/pdl_read_manager.c"
echo "OK $HERE/+x/pdl_read_manager.+x"
