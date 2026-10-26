#!/bin/sh
# build.sh - compile the world-ops op (wo.+x). The events call it by its widget path ($D/&.widgits/world-ops/ops/+x/wo.+x).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=gnu11 -Wall -Wextra -Wno-unused-result -O2 -o "$HERE/+x/wo.+x" "$HERE/wo.c"
echo "OK $HERE/+x/wo.+x"
