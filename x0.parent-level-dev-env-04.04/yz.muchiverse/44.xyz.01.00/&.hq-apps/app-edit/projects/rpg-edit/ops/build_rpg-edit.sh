#!/bin/sh
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/redit_op.+x redit_op.c && echo "OK +x/redit_op.+x"
