#!/bin/sh
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -O2 -o +x/exam_taker.+x exam_taker.c
echo "OK +x/exam_taker.+x"
