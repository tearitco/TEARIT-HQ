#!/bin/sh
# build_dust_check.sh - build ops/+x/dust_check.+x and ops/+x/dust_seed.+x (rpg-dust R0/R1). Run from anywhere.
HERE="$(cd "$(dirname "$0")" && pwd)"; SHARED="$HERE/../../../&.widgits/_shared-lib"
mkdir -p "$HERE/+x" || exit 1
for op in dust_check dust_seed dust_gen dust_zoom; do
  gcc -std=gnu11 -Wall -Wextra -Werror -O2 -I"$SHARED" -o "$HERE/+x/$op.+x" "$HERE/$op.c" || exit 1
  echo "OK $HERE/+x/$op.+x"
done
