#!/bin/sh
# build_dust_check.sh - build ops/+x/dust_check.+x (R0 checks of rpg-dust). Run from anywhere.
HERE="$(cd "$(dirname "$0")" && pwd)"; SHARED="$HERE/../../../&.widgits/_shared-lib"
mkdir -p "$HERE/+x" && gcc -std=gnu11 -Wall -Wextra -Werror -O2 -I"$SHARED" -o "$HERE/+x/dust_check.+x" "$HERE/dust_check.c" && echo "OK $HERE/+x/dust_check.+x"
