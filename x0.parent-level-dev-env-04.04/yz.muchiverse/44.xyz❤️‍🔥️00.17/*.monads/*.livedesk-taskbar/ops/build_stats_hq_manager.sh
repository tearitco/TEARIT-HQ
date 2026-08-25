#!/bin/sh
# build_stats_hq_manager.sh — build stats-hq's real MANAGER binary
# (stats_hq_manager.c), same pairing/build shape as
# build_db_hq_manager.sh (2026-08-25 full compliant rebuild - see
# stats_hq_manager.c's own header comment). No X11/Xft dependency -
# this binary never opens a window, it only reads/writes plain files.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- stats-hq manager -> +x/stats_hq_manager.+x"
$CC $CFLAGS -o +x/stats_hq_manager.+x stats_hq_manager.c

echo "OK +x/stats_hq_manager.+x"
