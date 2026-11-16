#!/bin/sh
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/tsc_op.+x tsc_op.c -lm && echo "OK +x/tsc_op.+x"
