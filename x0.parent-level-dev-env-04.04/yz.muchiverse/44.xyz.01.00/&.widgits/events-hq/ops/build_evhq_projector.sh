#!/bin/sh
# build_evhq_projector.sh - compile the events-hq UI projector.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/evhq_projector.+x" "$HERE/evhq_projector.c"
echo "OK $HERE/+x/evhq_projector.+x"
