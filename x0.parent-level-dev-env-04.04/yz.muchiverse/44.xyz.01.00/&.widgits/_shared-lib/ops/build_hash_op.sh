#!/bin/sh
# build_hash_op.sh - builds hash_op into +x/ (SHA-256 and the piece wallet id as a tiny op; links libcrypto like sig_op).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/hash_op.+x hash_op.c -lcrypto
echo "OK +x/hash_op.+x"
