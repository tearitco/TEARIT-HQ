#!/bin/sh
# builds ops/+x/dust_orbit.+x
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -Wall -Wextra -Werror -O2 -o +x/dust_orbit.+x dust_orbit.c -lm && echo OK
