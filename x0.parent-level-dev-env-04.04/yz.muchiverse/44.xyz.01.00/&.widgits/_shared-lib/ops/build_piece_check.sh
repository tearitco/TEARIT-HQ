#!/bin/sh
# build_piece_check.sh - builds piece_check into +x/ (read-only audit of piece identity across games and the Master Pal List).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/piece_check.+x piece_check.c
echo "OK +x/piece_check.+x"
