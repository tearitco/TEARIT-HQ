#!/bin/sh
# build_csv_lab_manager.sh - compile the csv-lab-hq window backend (self-contained .c) into ops/+x/ (git-ignored).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-format-truncation -O2 \
    -o "$HERE/+x/csv_lab_manager.+x" "$HERE/csv_lab_manager.c"
echo "OK $HERE/+x/csv_lab_manager.+x"
