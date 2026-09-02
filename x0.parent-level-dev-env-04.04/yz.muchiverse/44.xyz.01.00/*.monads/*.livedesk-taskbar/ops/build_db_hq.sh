#!/bin/sh
# build_db_hq.sh — build the db-hq standalone renderer (khtpm_hq_render.c +
# khtpm_css_parser.c), the first proof of the HQML CSS layer
# (au11-hq/HQML-DESIGN+PLANS.md). Separate from build_khtpm_strip.sh on
# purpose - db-hq is its own binary/process, not part of the taskbar pair,
# so a db-hq build/rebuild never touches or risks the taskbar's own two
# binaries.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
# macOS leg: XQuartz's Xft.pc lives under /opt/X11/lib/pkgconfig, invisible
# to brew's pkg-config by default; guarded — Linux behavior unchanged.
if [ "$(uname -s)" = "Darwin" ]; then
    PKG_CONFIG_PATH="/opt/X11/lib/pkgconfig:/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    export PKG_CONFIG_PATH
    X11_FLAGS="-I/opt/X11/include -L/opt/X11/lib"
else
    X11_FLAGS=""
fi
CFLAGS="-std=c11 -Wall -O2 $(pkg-config --cflags xft)"
LIBS="-lX11 $(pkg-config --libs xft) -lm"

# Sync shared files from the single canonical source (2026-08-12
# dedup pass - see &.widgits/_shared-lib/README.md).
SHARED="$(cd "$(dirname "$0")/../../../&.widgits/_shared-lib" && pwd)"
cp "$SHARED/khtpm_css_parser.c" khtpm_css_parser.c
cp "$SHARED/khtpm_css_parser.h" khtpm_css_parser.h
cp "$SHARED/khtpm_render_core.c" khtpm_render_core.c
mkdir -p lib
cp "$SHARED/stb_image_write.h" lib/stb_image_write.h

echo "-- db-hq renderer -> +x/khtpm_hq_render.+x"
$CC $CFLAGS $X11_FLAGS -o +x/khtpm_hq_render.+x \
  khtpm_hq_render.c khtpm_css_parser.c khtpm_taskbar_manager.c $LIBS

echo "OK +x/khtpm_hq_render.+x"
