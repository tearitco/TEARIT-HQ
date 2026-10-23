#!/bin/sh
# build_rpg_edit_hq.sh - compile ops/rpg_edit_hq.c (a skin of ../../../&.widgits/_shared-lib/petslike_core.c; -I. so the core can re-include this file) to ops/+x/rpg_edit_hq.+x (stb_image is text-included from &.widgits/_shared-lib; -lm only).
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter  -I. -o +x/rpg_edit_hq.+x rpg_edit_hq.c -lm && echo "OK $(pwd)/+x/rpg_edit_hq.+x"
