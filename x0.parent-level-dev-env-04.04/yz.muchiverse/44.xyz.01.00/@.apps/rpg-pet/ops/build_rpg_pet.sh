#!/bin/sh
# build_rpg_pet.sh - compile ops/rpg_pet.c to ops/+x/rpg_pet.+x (stb_image is text-included from &.widgits/_shared-lib; -lm only).
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter -o +x/rpg_pet.+x rpg_pet.c -lm && echo "OK $(pwd)/+x/rpg_pet.+x"
