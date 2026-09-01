#!/bin/sh
# build_core_render.sh — build khtpm_core_render.c, Stage 2c PROOF
# (ONE-entity test case, see local-2do-15.txt's own entry). Same real
# shared-source convention as build_db_hq.sh - not invented.
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
# REAL, NEW 2026-09-01 - -lXext added for the tile mode's own real
# X11 Shape Extension use (XShapeCombineMask, folded in verbatim from
# tp_desktop_window_rgb.c's build_shape_mask()/cursword_update_shape()).
LIBS="-lX11 -lXext $(pkg-config --libs xft) -lm"

SHARED="$(cd "$(dirname "$0")/../../../&.widgits/_shared-lib" && pwd)"
cp "$SHARED/khtpm_css_parser.c" khtpm_css_parser.c
cp "$SHARED/khtpm_css_parser.h" khtpm_css_parser.h
cp "$SHARED/khtpm_render_core.c" khtpm_render_core.c
cp "$SHARED/khtpm_draw_core.c" khtpm_draw_core.c
mkdir -p lib
cp "$SHARED/stb_image_write.h" lib/stb_image_write.h

# REAL Stage 1 follow-up (2026-08-16) - dump_frame_png_op.+x is a real,
# standalone, shared op binary (system()-invoked, not text-included -
# see khtpm-merge-how2.md's own "HOUSE STANDARD" section), build it
# once, centrally, if missing.
echo "-- swatch_picker_manager -> +x/swatch_picker_manager.+x"
$CC -std=c11 -Wall -O2 -o +x/swatch_picker_manager.+x swatch_picker_manager.c
OPS_BIN="$SHARED/ops/+x/dump_frame_png_op.+x"
if [ ! -x "$OPS_BIN" ]; then
  (cd "$SHARED/ops" && sh build_dump_frame_png_op.sh)
fi

# REAL FIX 2026-09-01 - khtpm_taskbar_manager.c dropped from this link
# line: real, confirmed dead (the ktb_init()/ktb_quit_and_save() calls
# this used to exist for were already removed from khtpm_core_render.c
# in an earlier pass this same session; this file builds and links
# clean without it). Also the real house standard, per khtpm-merge-
# how2.md's own "HOUSE STANDARD" section and confirmed directly again
# this session: no cross-.c linking to share behavior within one
# binary - genuinely the same file, or a separate fork/exec+file-IPC
# process (khtpm_taskbar_manager_main.+x's own real, separate compile
# of khtpm_taskbar_manager.c is that legitimate case, untouched).
echo "-- entity-menu renderer -> +x/khtpm_core_render.+x"
$CC $CFLAGS $X11_FLAGS -o +x/khtpm_core_render.+x \
  khtpm_core_render.c khtpm_css_parser.c $LIBS

echo "OK +x/khtpm_core_render.+x"
