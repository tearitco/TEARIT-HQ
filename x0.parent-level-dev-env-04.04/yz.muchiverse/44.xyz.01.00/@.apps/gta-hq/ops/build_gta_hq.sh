#!/bin/sh
# build_gta_hq.sh - compile ops/gta_hq.c to ops/+x/gta_hq.+x (stb_image is text-included from &.widgits/_shared-lib; -lm only).
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter -o +x/gta_hq.+x gta_hq.c -lm && echo "OK $(pwd)/+x/gta_hq.+x"
