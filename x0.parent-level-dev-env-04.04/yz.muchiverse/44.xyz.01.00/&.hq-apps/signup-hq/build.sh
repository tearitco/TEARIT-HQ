#!/bin/sh
# build.sh - build signup-hq's manager (no Elem/X11 dependency; it only
# writes a .chtpm projection the shared renderer picks up).
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$SDIR/+x" "$SDIR/ops/+x"
CC=${CC:-gcc}
echo "-- signup_hq_manager -> +x/signup_hq_manager.+x"
$CC -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$SDIR/+x/signup_hq_manager.+x" "$SDIR/signup_hq_manager.c" \
  && echo "OK signup_hq_manager" || exit 1
