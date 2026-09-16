#!/bin/sh
# build.sh - build export-hq's manager (no Elem/X11 dependency; it only
# writes a state/ui.txt projection the shared renderer picks up).
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$SDIR/+x"
CC=${CC:-gcc}
echo "-- export_hq_manager -> +x/export_hq_manager.+x"
$CC -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$SDIR/+x/export_hq_manager.+x" "$SDIR/export_hq_manager.c" \
  && echo "OK export_hq_manager" || exit 1
