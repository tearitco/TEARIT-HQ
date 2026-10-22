#!/bin/sh
# build_mcl_hq.sh - compile ops/mcl_hq.c to ops/+x/mcl_hq.+x (stb_image is text-included from &.widgits/_shared-lib; -lm only).
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter  -I. -o +x/mcl_hq.+x mcl_hq.c -lm && echo "OK $(pwd)/+x/mcl_hq.+x"
