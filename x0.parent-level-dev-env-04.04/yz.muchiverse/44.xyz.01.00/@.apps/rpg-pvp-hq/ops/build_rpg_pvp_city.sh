#!/bin/sh
# build_rpg_pvp_city.sh - compile ops/rpg_pvp_city.c to ops/+x/rpg_pvp_city.+x (stb_image_write is text-included from &.widgits/_shared-lib; -lm only).
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter -o +x/rpg_pvp_city.+x rpg_pvp_city.c -lm && echo "OK $(pwd)/+x/rpg_pvp_city.+x"
