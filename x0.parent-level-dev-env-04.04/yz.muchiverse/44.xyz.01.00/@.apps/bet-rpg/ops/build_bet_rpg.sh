#!/bin/sh
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-format-truncation -o +x/bet_rpg.+x bet_rpg.c -lm && echo "OK $(pwd)/+x/bet_rpg.+x"
