#!/bin/sh
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/texting_op.+x texting_op.c && echo "OK +x/texting_op.+x"
