#!/bin/sh
# build_life_op.sh - builds life_op into +x/ (the life layer hunger/starvation clock as an op; slice 1 of the petslike_life.c migration).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/life_op.+x life_op.c
echo "OK +x/life_op.+x"
