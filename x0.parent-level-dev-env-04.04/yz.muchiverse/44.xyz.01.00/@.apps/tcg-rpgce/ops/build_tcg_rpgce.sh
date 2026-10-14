#!/bin/sh
# build_tcg_rpgce.sh - compile ops/tcg_rpgce.c to ops/+x/tcg_rpgce.+x (stb_image is text-included from &.widgits/_shared-lib; -lm only).
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter -o +x/tcg_rpgce.+x tcg_rpgce.c -lm && echo "OK $(pwd)/+x/tcg_rpgce.+x"
