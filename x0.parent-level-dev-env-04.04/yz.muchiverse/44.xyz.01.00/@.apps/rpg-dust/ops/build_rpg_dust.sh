#!/bin/sh
# build_rpg_dust.sh - compile ops/rpg_dust.c (a skin of ../../../&.widgits/_shared-lib/petslike_core.c; -I. so the core can re-include this file) to ops/+x/rpg_dust.+x, and the world ops it calls (build_dust_check.sh: dust_gen, dust_zoom, ...).
cd "$(dirname "$0")" && sh ./build_dust_check.sh >/dev/null || exit 1
mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter -I. -o +x/rpg_dust.+x rpg_dust.c -lm && echo "OK $(pwd)/+x/rpg_dust.+x"
