#!/bin/sh
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/stockx_op.+x stockx_op.c && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/stockx_fetch_op.+x stockx_fetch_op.c -lm && echo "OK +x/stockx_op.+x +x/stockx_fetch_op.+x"
