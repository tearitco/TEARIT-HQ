#!/bin/sh
# build_phys_node_eval.sh - builds the compiled ops of this folder into +x/ (picked up by $.crypts/compile-runner.sh as ops/build_*.sh). Added 2026-10-07:
# these ops were compiled by hand during testing and no build script covered them, so a fresh checkout had no binaries for the harness/games.
set -e
cd "$(dirname "$0")"
mkdir -p +x
for n in phys_node_eval; do
    ${CC:-gcc} -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-unused-result -O2 -o "+x/$n.+x" "$n.c" -lm
    echo "OK +x/$n.+x"
done
