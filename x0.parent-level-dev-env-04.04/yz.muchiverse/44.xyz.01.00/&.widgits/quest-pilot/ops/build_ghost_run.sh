#!/bin/sh
# build_ghost_run.sh - compile ghost_run (self-contained C, no shared headers; fork+exec only).
# -Wno-unknown-warning-option comes first so clang on macOS ignores the gcc-only -Wno-* after it.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
$CC -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-format-truncation -O2 -o +x/ghost_run.+x ghost_run.c
echo "OK +x/ghost_run.+x"
