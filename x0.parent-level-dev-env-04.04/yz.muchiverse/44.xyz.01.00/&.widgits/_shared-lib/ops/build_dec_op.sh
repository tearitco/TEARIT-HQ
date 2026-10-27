#!/bin/sh
# build_dec_op.sh - builds dec_op into +x/ (decision-ledger digest: bank, review, rotation, pick chance; replaces the text-include game_decisions.c).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/dec_op.+x dec_op.c
echo "OK +x/dec_op.+x"
