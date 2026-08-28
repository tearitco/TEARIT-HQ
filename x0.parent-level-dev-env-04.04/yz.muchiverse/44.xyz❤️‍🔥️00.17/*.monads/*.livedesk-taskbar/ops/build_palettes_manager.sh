#!/bin/sh
# build_palettes_manager.sh — build palettes' real MANAGER binary
# (palettes_manager.c), same pairing/build shape as build_stats_hq_
# manager.sh / build_bookmarks_manager.sh (2026-08-25, TPMOS-COMPLIANCE-
# DEBT.md's own standing rule). No X11/Xft dependency.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- palettes manager -> +x/palettes_manager.+x"
# REAL FIX 2026-08-27 (TILE-SYSTEM-DESIGN.md sec.4b, "rmmv" category
# now uses stb_image.h to crop real tile PNGs - stb_image's PNG decode
# path calls libm's pow(), needs -lm linked; no other category needed
# it before this).
$CC $CFLAGS -o +x/palettes_manager.+x palettes_manager.c -lm

echo "OK +x/palettes_manager.+x"
