#!/bin/sh
# build_board_vars_op.sh - builds +x/board_vars_op.+x (-Wall -Wextra). Check: sh ../verify.sh
set -e
cd "$(dirname "$0")"; mkdir -p +x
${CC:-gcc} -std=c11 -Wall -Wextra -O2 -o +x/board_vars_op.+x board_vars_op.c
echo "OK +x/board_vars_op.+x"
