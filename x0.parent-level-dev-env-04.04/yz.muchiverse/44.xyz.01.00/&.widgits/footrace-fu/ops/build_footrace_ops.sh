#!/bin/sh
# build_footrace_ops.sh - builds the compiled ops of this folder into +x/ (picked up by $.crypts/compile-runner.sh as ops/build_*.sh). Added 2026-10-07:
# these ops were compiled by hand during testing and no build script covered them, so a fresh checkout had no binaries for the harness/games.
set -e
cd "$(dirname "$0")"
mkdir -p +x
for n in rand_roll row_append var_find; do
    ${CC:-gcc} -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-unused-result -O2 -o "+x/$n.+x" "$n.c" 
    echo "OK +x/$n.+x"
done
