#!/bin/sh
# build_layout_flow.sh - builds ops/+x/layout_flow.+x
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -D_GNU_SOURCE -o +x/layout_flow.+x layout_flow.c
