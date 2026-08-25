#!/bin/sh
# build_events_hq_manager.sh — build events-hq's MANAGER binary
# (khtpm_events_hq_manager.c), Stage 2d shell/manager split, same real
# mechanism proven on db-hq (see khtpm_hq_manager.c's own build script).
# No X11/Xft dependency - this binary never opens a window.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- events-hq manager -> +x/khtpm_events_hq_manager.+x"
$CC $CFLAGS -o +x/khtpm_events_hq_manager.+x khtpm_events_hq_manager.c

echo "OK +x/khtpm_events_hq_manager.+x"
