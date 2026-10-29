#!/bin/sh
# build_rpg_pvp_hq.sh - compile ops/rpg_pvp_hq.c (a skin of &.widgits/_shared-lib/petslike_core.c) to ops/+x/rpg_pvp_hq.+x
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter  -I. -o +x/rpg_pvp_hq.+x rpg_pvp_hq.c -lm && echo "OK $(pwd)/+x/rpg_pvp_hq.+x"
