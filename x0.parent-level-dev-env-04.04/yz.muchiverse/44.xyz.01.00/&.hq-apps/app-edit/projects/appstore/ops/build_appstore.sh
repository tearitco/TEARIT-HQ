#!/bin/sh
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/store_op.+x store_op.c && echo "OK +x/store_op.+x"
