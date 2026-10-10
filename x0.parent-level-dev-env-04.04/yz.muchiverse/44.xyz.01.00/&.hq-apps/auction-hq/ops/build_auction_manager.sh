#!/bin/sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=gnu11 -Wall -Wextra -Wno-format-truncation -O2 -o "$HERE/+x/auction_manager.+x" "$HERE/auction_manager.c"
echo "OK $HERE/+x/auction_manager.+x"
