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
LIBS="-lX11 -lXext $(pkg-config --libs xft) $(pkg-config --libs fontconfig) -lm"

# SHARED-SOURCE-COMPILE-IN-PLACE.md (2026-09-09): the house-authored
# shared .c/.h are NO LONGER copied into this dir. `-I "$SHARED"`
# points the include search at the one canonical copy, and
# khtpm_css_parser.c is named by its canonical path on the compile
# line below. Text-includes (#include "khtpm_render_core.c" /
# "khtpm_draw_core.c" inside khtpm_core_render.c) resolve via the same
# -I. Only the built binary stays local. stb_image_write.h is a frozen
# vendored third-party header (not house code, ~zero drift risk) and is
# still copied for now — out of scope for this pass.
SHARED="$(cd "$(dirname "$0")/../../../&.widgits/_shared-lib" && pwd)"

# 2026-10-08: a stale same-named copy in THIS dir shadows $SHARED, because
# `#include "x.h"` looks beside the including file before -I. It broke the
# build once (ops/khtpm_css_parser.h lacked the new css_len/g_css_ui_pct).
# The shared files are compiled in place, so any local copy of one is
# vestigial: delete it instead of letting it win.
for _f in "$SHARED"/khtpm_*.[ch] "$SHARED"/kh_*.[ch]; do
    _b="$(basename "$_f")"
    [ -f "./$_b" ] && ! [ "./$_b" -ef "$_f" ] && { echo "build: removing stale shadow copy ./$_b (compiled from $SHARED)"; rm -f "./$_b"; }
done
mkdir -p lib
cp "$SHARED/stb_image_write.h" lib/stb_image_write.h

# REAL, NEW 2026-09-28 (EVENT-MODULARITY-AND-BUILD-SPEED.md §2, direct
# instruction: "its embarrassing to have such slow compile time...
# track last changed files thru a stored hash and only compile those").
# See hash_gate.sh's own header for the full design (per-binary
# combined hash over all real inputs, one manifest per project).
MANIFEST="$(dirname "$0")/.build_hashes.pdl"
. "$SHARED/hash_gate.sh"

# REAL Stage 1 follow-up (2026-08-16) - dump_frame_png_op.+x is a real,
# standalone, shared op binary (system()-invoked, not text-included -
# see khtpm-merge-how2.md's own "HOUSE STANDARD" section), build it
# once, centrally, if missing.
#
# REAL FIX 2026-09-28 (direct instruction: "extend gate and not ignore
# it" - these three were the last ungated compiles in this script,
# recompiling unconditionally on every single build regardless of
# whether their source changed). Now hash-gated the same as
# khtpm_core_render.+x/khtpm_entity.+x below - same MANIFEST, same
# hash_gate.sh already sourced above this block.
if hash_gate_stale "$MANIFEST" +x/swatch_picker_manager.+x swatch_picker_manager.c; then
    echo "-- swatch_picker_manager -> +x/swatch_picker_manager.+x"
    $CC -std=c11 -Wall -O2 -o +x/swatch_picker_manager.+x swatch_picker_manager.c
    hash_gate_commit "$MANIFEST" +x/swatch_picker_manager.+x swatch_picker_manager.c
else
    echo "-- swatch_picker_manager.+x up to date (hash unchanged), skipping compile"
fi
# apply_theme_op.+x - the standalone op swatch_picker_manager exec()s on a
# swatch pick. Had no build hook (built by hand once); added here so
# $.restart keeps it fresh alongside its only caller.
if hash_gate_stale "$MANIFEST" +x/apply_theme_op.+x apply_theme_op.c; then
    echo "-- apply_theme_op -> +x/apply_theme_op.+x"
    $CC -std=c11 -Wall -O2 -o +x/apply_theme_op.+x apply_theme_op.c
    hash_gate_commit "$MANIFEST" +x/apply_theme_op.+x apply_theme_op.c
else
    echo "-- apply_theme_op.+x up to date (hash unchanged), skipping compile"
fi
# ktb_zorder_op.+x - process-management half of the "@" always-on-top toggle
# (dock unfactor stage 2, 2026-09-20): the renderer spawns it detached on
# ZORDER_TOGGLE. See ktb_zorder_op.c's header + DOCK-UNFACTOR-AUDIT.md.
if hash_gate_stale "$MANIFEST" +x/ktb_zorder_op.+x ktb_zorder_op.c; then
    echo "-- ktb_zorder_op -> +x/ktb_zorder_op.+x"
    $CC -std=c11 -Wall -O2 $X11_FLAGS -o +x/ktb_zorder_op.+x ktb_zorder_op.c -lX11
    hash_gate_commit "$MANIFEST" +x/ktb_zorder_op.+x ktb_zorder_op.c
else
    echo "-- ktb_zorder_op.+x up to date (hash unchanged), skipping compile"
fi
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
# Q003 (2026-10-06): every file khtpm_core_render.c text-includes must be listed, or editing it silently leaves a STALE binary running (hash_gate.sh: "pass every real input
# file explicitly"). Added: khtpm_css_parser.h, khtpm_nav_echo.c, house_wait.h, kh_boot_mark.h, kh_proc_registry.h, stb_image.h, stb_image_write.h. Scorer: ^.grave/quests/Q003-build-gate-include-list/verify.sh
CR_SRCS="khtpm_core_render.c $SHARED/khtpm_css_parser.c $SHARED/khtpm_css_parser.h $SHARED/khtpm_ui_scale.c $SHARED/khtpm_render_core.c $SHARED/khtpm_draw_core.c $SHARED/khtpm_reparse_diff.c $SHARED/khtpm_nav_echo.c $SHARED/house_wait.h $SHARED/kh_boot_mark.h $SHARED/kh_proc_registry.h $SHARED/stb_image.h $SHARED/stb_image_write.h"
if hash_gate_stale "$MANIFEST" +x/khtpm_core_render.+x $CR_SRCS; then
    echo "-- entity-menu renderer -> +x/khtpm_core_render.+x"
    $CC $CFLAGS $X11_FLAGS -I "$SHARED" -o +x/khtpm_core_render.+x \
      khtpm_core_render.c "$SHARED/khtpm_css_parser.c" "$SHARED/khtpm_ui_scale.c" $LIBS
    hash_gate_commit "$MANIFEST" +x/khtpm_core_render.+x $CR_SRCS
    echo "OK +x/khtpm_core_render.+x"
else
    echo "-- khtpm_core_render.+x up to date (hash unchanged), skipping compile"
fi

# khtpm_entity.c is the real pal process source (tp_main and its tile/
# sprite/popup code). It needs neither the CSS parser nor the Elem/render
# core. Its former khtpm_ui_common.c text-include was inlined directly into
# khtpm_entity.c (2026-09-27 unfactor, TODO-khtpm_ui_common-unfactor.md) -
# it was never actually shared with khtpm_core_render.c despite this
# comment's old claim (grep confirmed only khtpm_entity.c ever included it),
# so there was no real sharing left to preserve via -I "$SHARED".
if hash_gate_stale "$MANIFEST" +x/khtpm_entity.+x khtpm_entity.c; then
    echo "-- entity pal renderer -> +x/khtpm_entity.+x"
    $CC $CFLAGS $X11_FLAGS -I "$SHARED" -I . -o +x/khtpm_entity.+x \
      khtpm_entity.c $LIBS
    hash_gate_commit "$MANIFEST" +x/khtpm_entity.+x khtpm_entity.c
    echo "OK +x/khtpm_entity.+x"
else
    echo "-- khtpm_entity.+x up to date (hash unchanged), skipping compile"
fi
