#!/bin/sh
# builds layout_op (needs -lX11)
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -Wall -O2 -o +x/layout_op.+x layout_op.c -lX11 && echo "OK +x/layout_op.+x"
