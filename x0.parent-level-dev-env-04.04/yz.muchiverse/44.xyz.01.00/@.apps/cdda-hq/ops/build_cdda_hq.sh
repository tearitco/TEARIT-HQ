#!/bin/sh
# build_cdda_hq.sh - compile ops/cdda_hq.c to ops/+x/cdda_hq.+x (stb_image is text-included from &.widgits/_shared-lib; -lm only).
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter  -I. -o +x/cdda_hq.+x cdda_hq.c -lm && echo "OK $(pwd)/+x/cdda_hq.+x"
