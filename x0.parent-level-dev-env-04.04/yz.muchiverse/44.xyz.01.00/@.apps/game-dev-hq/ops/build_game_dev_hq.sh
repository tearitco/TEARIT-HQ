#!/bin/sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -O2 -o "$HERE/+x/game_dev_hq.+x" "$HERE/game_dev_hq.c" "$HERE/office_book.c" -lm
echo "OK $HERE/+x/game_dev_hq.+x"
