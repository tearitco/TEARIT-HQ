#!/bin/sh
# build_open_hai_manager.sh — build open-hai's MANAGER binary
# (khtpm_open_hai_manager.c), Stage 2d shell/manager split, same real
# mechanism proven on db-hq/events-hq/chat-hai (see khtpm_hq_manager.c's
# own build script). No X11/Xft dependency - this binary never opens a
# window, it only reads/writes plain files + forks curl/tool jobs.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- open-hai manager -> +x/khtpm_open_hai_manager.+x"
$CC $CFLAGS -o +x/khtpm_open_hai_manager.+x khtpm_open_hai_manager.c

echo "OK +x/khtpm_open_hai_manager.+x"
