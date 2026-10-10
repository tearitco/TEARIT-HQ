#!/bin/sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=gnu11 -Wall -Wextra -Wno-format-truncation -O2 -o "$HERE/+x/exchange_manager.+x" "$HERE/exchange_manager.c" -lm
echo "OK $HERE/+x/exchange_manager.+x"
