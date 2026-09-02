#!/bin/sh
# build_dump_frame_png_op.sh — build the real, standalone dump_frame_png_op
# binary (2026-08-16 TPMOS-standard correction). Real op binary, invoked
# via fork/exec or system() by any khtpm app - never text-included.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"
LIBS="-lX11 -lm"

echo "-- dump_frame_png_op -> +x/dump_frame_png_op.+x"
$CC $CFLAGS -o +x/dump_frame_png_op.+x dump_frame_png_op.c $LIBS
echo "OK +x/dump_frame_png_op.+x"
