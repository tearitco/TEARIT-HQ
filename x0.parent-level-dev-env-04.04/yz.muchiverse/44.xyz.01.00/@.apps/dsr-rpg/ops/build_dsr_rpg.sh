#!/bin/sh
# build_dsr_rpg.sh - compile ops/dsr_rpg.c to ops/+x/dsr_rpg.+x (stb_image is text-included from &.widgits/_shared-lib; -lm only).
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter -o +x/dsr_rpg.+x dsr_rpg.c -lm && echo "OK $(pwd)/+x/dsr_rpg.+x"
