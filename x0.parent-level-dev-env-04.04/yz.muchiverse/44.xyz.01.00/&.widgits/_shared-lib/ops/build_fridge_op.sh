#!/bin/sh
# build_fridge_op.sh - builds fridge_op into +x/ (the ship's fridge: stock, a hungry character eats one food, ledger row).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/fridge_op.+x fridge_op.c
echo "OK +x/fridge_op.+x"
