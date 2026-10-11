#!/bin/sh
# build_desk_copy_op.sh - builds desk_copy_op into +x/ (copy a desk and its entities under new names; verify: the pal harness harness/desk_copy.pal).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=c11 -Wall -Wextra -O2 -o +x/desk_copy_op.+x desk_copy_op.c
echo "OK +x/desk_copy_op.+x"
