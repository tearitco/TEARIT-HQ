#!/bin/sh
# builds tools/+x/board_ctl.+x
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -Wall -Wextra -O2 -o +x/board_ctl.+x board_ctl.c && echo OK
