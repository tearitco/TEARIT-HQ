#!/bin/sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -O2 \
    -o "$HERE/+x/dbhq_ce_bridge.+x" "$HERE/dbhq_ce_bridge.c"
echo "OK $HERE/+x/dbhq_ce_bridge.+x"
