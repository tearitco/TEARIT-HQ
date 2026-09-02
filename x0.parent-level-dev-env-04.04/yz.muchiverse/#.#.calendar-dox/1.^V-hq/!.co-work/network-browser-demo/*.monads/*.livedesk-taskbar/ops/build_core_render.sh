#!/bin/sh
# build_core_render.sh — builds khtpm_core_render.c, the REAL, complete,
# unmodified shared house renderer (18k+ lines - it also serves 7 other
# apps in the real house: db-hq, events-hq, chat-hai, open-hai,
# palettes, bookmarks, stats-hq, taskbar-settings. None of those code
# paths are reachable from this demo's own real invocation shape -
# <house_root> <chtpm_path>, argc==3, the generic default/sidebar+panel
# mode network-browser itself uses - but they're real, live code in
# this same binary, same as in the actual house).
#
# Adapted from the real house's own build_core_render.sh for this
# package: the house copies khtpm_css_parser.c/.h/khtpm_render_core.c/
# khtpm_draw_core.c/stb_image_write.h from its own internal
# &.widgits/_shared-lib/ at build time - this package copies from the
# sibling ../../../khtpm-core/ folder instead, same real "copy, don't
# hand-fork" convention, just pointed at this repo's own canonical copy.
#
# Left out (real, but genuinely unrelated to running THIS app):
#   - swatch_picker_manager.+x - a separate, unrelated small op binary
#     the house's own build script builds opportunistically here; not
#     part of khtpm_core_render.+x itself.
#   - dump_frame_png_op.+x - a real, optional screenshot/debug helper
#     some code paths shell out to; harmless if missing (same "op
#     binary absent = no-op" convention this house uses everywhere).
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
if [ "$(uname -s)" = "Darwin" ]; then
    PKG_CONFIG_PATH="/opt/X11/lib/pkgconfig:/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    export PKG_CONFIG_PATH
    X11_FLAGS="-I/opt/X11/include -L/opt/X11/lib"
else
    X11_FLAGS=""
fi
CFLAGS="-std=c11 -Wall -O2 $(pkg-config --cflags xft)"
LIBS="-lX11 -lXext $(pkg-config --libs xft) -lm"

SHARED="$(cd "$(dirname "$0")/../../../../khtpm-core" && pwd)"
cp "$SHARED/khtpm_css_parser.c" khtpm_css_parser.c
cp "$SHARED/khtpm_css_parser.h" khtpm_css_parser.h
cp "$SHARED/khtpm_render_core.c" khtpm_render_core.c
cp "$SHARED/khtpm_draw_core.c" khtpm_draw_core.c
mkdir -p lib
cp "$SHARED/stb_image_write.h" lib/stb_image_write.h

echo "-- khtpm_core_render (the real, complete house renderer) -> +x/khtpm_core_render.+x"
$CC $CFLAGS $X11_FLAGS -o +x/khtpm_core_render.+x \
  khtpm_core_render.c khtpm_css_parser.c $LIBS

echo "OK +x/khtpm_core_render.+x"
