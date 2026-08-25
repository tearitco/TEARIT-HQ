#!/bin/sh
# build_bookmarks_manager.sh — build bookmarks' real MANAGER binary
# (bookmarks_manager.c), same pairing/build shape as
# build_stats_hq_manager.sh (2026-08-25, TPMOS-COMPLIANCE-DEBT.md's own
# standing rule - see bookmarks_manager.c's own header comment). No
# X11/Xft dependency - this binary never opens a window.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- bookmarks manager -> +x/bookmarks_manager.+x"
$CC $CFLAGS -o +x/bookmarks_manager.+x bookmarks_manager.c

echo "OK +x/bookmarks_manager.+x"
