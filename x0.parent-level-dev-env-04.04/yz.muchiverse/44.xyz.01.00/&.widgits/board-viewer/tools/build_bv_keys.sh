#!/bin/sh
# builds tools/+x/bv_keys.+x
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -Wall -Wextra -O2 -o +x/bv_keys.+x bv_keys.c && echo OK
