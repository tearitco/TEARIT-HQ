#!/bin/sh
# build_kh_proc_register_op.sh — build the real, standalone
# kh_proc_register_op binary (shell-callable proc-registry wrapper for
# launcher scripts, 2026-09-15).
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- kh_proc_register_op -> +x/kh_proc_register_op.+x"
$CC $CFLAGS -o +x/kh_proc_register_op.+x kh_proc_register_op.c
echo "OK +x/kh_proc_register_op.+x"
