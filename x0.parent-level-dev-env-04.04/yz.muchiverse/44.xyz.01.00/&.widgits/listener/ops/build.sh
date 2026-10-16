#!/bin/bash
# build.sh - listener ops (pure C). Output: ops/+x/
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"; mkdir -p "$SDIR/+x"; CC=${CC:-gcc}
for op in vad wer; do $CC -std=gnu11 -Wall -Wno-unused-function -Wno-misleading-indentation -O2 -o "$SDIR/+x/$op.+x" "$SDIR/$op.c" -lm && echo "OK $op" || exit 1; done
for op in hear listen_capture; do [ -f "$SDIR/$op.c" ] && { $CC -std=gnu11 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-format-truncation -O2 -o "$SDIR/+x/$op.+x" "$SDIR/$op.c" -lm && echo "OK $op" || exit 1; }; done
exit 0
