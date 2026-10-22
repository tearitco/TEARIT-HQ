#!/bin/sh
# build_petslike_tpl.sh - compile petslike_tpl.c (the shared window templates of the pets-like games, PETSLIKE-FRAMEWORK-PLAN.md P2) to ops/+x/petslike_tpl.+x
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -o +x/petslike_tpl.+x petslike_tpl.c && echo "OK $(pwd)/+x/petslike_tpl.+x"
