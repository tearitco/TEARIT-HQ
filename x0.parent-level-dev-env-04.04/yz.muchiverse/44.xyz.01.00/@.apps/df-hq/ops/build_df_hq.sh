#!/bin/sh
# build_df_hq.sh - compile ops/df_hq.c to ops/+x/df_hq.+x (stb_image is text-included from &.widgits/_shared-lib; -lm only).
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter  -I. -o +x/df_hq.+x df_hq.c -lm && echo "OK $(pwd)/+x/df_hq.+x"
