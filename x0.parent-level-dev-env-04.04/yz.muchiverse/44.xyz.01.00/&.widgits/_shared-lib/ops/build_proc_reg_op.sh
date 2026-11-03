#!/bin/sh
# build_proc_reg_op.sh - build ops/+x/proc_reg_op.+x (the process registry v2 front end, kh_proc_reg.h). 2026-11-02.
set -e
cd "$(dirname "$0")"
mkdir -p +x
echo "-- proc_reg_op -> +x/proc_reg_op.+x"
${CC:-gcc} -std=gnu11 -Wall -Wextra -Wno-format-truncation -O2 -o +x/proc_reg_op.+x proc_reg_op.c
echo "OK +x/proc_reg_op.+x"
