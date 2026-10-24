#!/bin/sh
# builds tools/+x/worldscope.+x
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -Wall -Wextra -O2 -o +x/worldscope.+x worldscope.c && echo OK
