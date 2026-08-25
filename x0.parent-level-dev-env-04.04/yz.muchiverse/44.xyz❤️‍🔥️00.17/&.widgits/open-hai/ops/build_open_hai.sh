#!/bin/sh
# build_open_hai.sh — build the taskbar cell 14 ("ai") window.
set -e
cd "$(dirname "$0")"
mkdir -p +x

CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2 $(pkg-config --cflags xft)"
LIBS="-lX11 $(pkg-config --libs xft) -lm"

# NOTE: v1 uses hand-rolled pixel-math layout (same fallback shape
# db-hq/events-hq are built on), NOT the khtpm CSS engine yet - no
# khtpm_css_parser.c dependency to sync/compile this pass. Real CSS-
# driven styling is a documented follow-up, not done here (see
# OPEN-HAI-GUI-DESIGN.md §4 - the file still #includes the header for
# CssStyle types if a future pass wires it in properly).

# Sync stb_image_write.h from the single canonical source (2026-08-12
# dedup pass - see &.widgits/_shared-lib/README.md) - needed for
# dump_frame_png()'s real PNG+receipt verification (learn to rely on
# receipts, not external screen capture - see that function's own
# header comment).
SHARED="$(cd "$(dirname "$0")/../../_shared-lib" && pwd)"
mkdir -p lib
cp "$SHARED/stb_image_write.h" lib/stb_image_write.h

echo "-- open-hai renderer -> +x/khtpm_open_hai_render.+x"
$CC $CFLAGS -o +x/khtpm_open_hai_render.+x \
  khtpm_open_hai_render.c $LIBS

echo "OK +x/khtpm_open_hai_render.+x"
