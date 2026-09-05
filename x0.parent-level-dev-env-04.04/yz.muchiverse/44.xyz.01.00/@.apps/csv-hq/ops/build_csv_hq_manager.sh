#!/bin/sh
# build_csv_hq_manager.sh - compile csv-hq's own grid/file manager.
# Mirrors pdl-read/ops/build_pdl_read_manager.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/csv_hq_manager.+x" "$HERE/csv_hq_manager.c" -lm
echo "OK $HERE/+x/csv_hq_manager.+x"
