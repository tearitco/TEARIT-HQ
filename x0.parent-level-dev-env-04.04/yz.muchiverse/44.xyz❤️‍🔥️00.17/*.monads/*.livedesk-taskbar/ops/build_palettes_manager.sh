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
$CC $CFLAGS -o +x/palettes_manager.+x palettes_manager.c

echo "OK +x/palettes_manager.+x"
