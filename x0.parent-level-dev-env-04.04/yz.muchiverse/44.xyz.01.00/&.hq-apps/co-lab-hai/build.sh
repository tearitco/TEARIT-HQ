#!/bin/bash
# build.sh - builds co-lab-hai's real manager.
#   colab_hai_manager.+x - real manager (approval-gated multi-agent
#     chat log), publishes co-lab-hai.chtpm live. Zero Elem/X11
#     dependency, shared by the real window (button.sh -> shared
#     khtpm_core_render.+x's generic default path).
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$SDIR/+x" "$SDIR/ops/+x"
CC=${CC:-gcc}

echo "-- colab_hai_manager -> +x/colab_hai_manager.+x"
$CC -std=c11 -Wall -O2 -o "$SDIR/+x/colab_hai_manager.+x" "$SDIR/colab_hai_manager.c" && echo "OK colab_hai_manager" || exit 1
