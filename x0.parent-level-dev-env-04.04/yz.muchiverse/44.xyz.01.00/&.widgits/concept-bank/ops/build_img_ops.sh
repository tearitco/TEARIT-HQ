#!/bin/bash
# build_img_ops.sh - builds the image word-bank ops (img_prompt, img_edit) into ops/+x/. img_bank.c is text-included by both, never compiled alone.
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"; mkdir -p "$SDIR/+x"; CC=${CC:-gcc}
for op in img_prompt img_edit; do $CC -std=gnu11 -Wall -Wno-misleading-indentation -O2 -o "$SDIR/+x/$op.+x" "$SDIR/$op.c" -lm && echo "OK $op" || exit 1; done
