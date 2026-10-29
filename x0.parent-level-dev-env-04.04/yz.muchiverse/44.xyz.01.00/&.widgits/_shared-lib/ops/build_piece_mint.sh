#!/bin/sh
# build_piece_mint.sh - builds piece_mint and its sibling hash_op into +x/ (mints one piece into the Master Pal List; the wallet id comes from hash_op).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/piece_mint.+x piece_mint.c
[ -x +x/hash_op.+x ] || sh build_hash_op.sh
echo "OK +x/piece_mint.+x"
