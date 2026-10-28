#!/bin/sh
# builds tools/+x/bv_look.+x
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -Wall -Wextra -Werror -O2 -o +x/bv_look.+x bv_look.c -lm && echo OK
