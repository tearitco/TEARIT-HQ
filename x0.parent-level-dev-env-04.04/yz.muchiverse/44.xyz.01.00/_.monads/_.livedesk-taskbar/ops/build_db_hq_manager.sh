#!/bin/sh
# build_db_hq_manager.sh — build db-hq's MANAGER binary (khtpm_hq_manager.c),
# Stage 2d shell/manager split (au11-hq/khtpm-merge-how2.md +
# local-2do-15.txt's own "Stage 2d, REDONE correctly" entry, 2026-08-16).
# Separate from build_db_hq.sh on purpose - the shell (khtpm_hq_render.c)
# and this manager are independent standalone binaries, launched/killed
# together by open_db_hq.sh, same pairing shape as khtpm_strip_parser.c +
# khtpm_taskbar_manager_main.c. No X11/Xft dependency at all - this
# binary never opens a window, it only reads/writes plain files.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- db-hq manager -> +x/khtpm_hq_manager.+x"
$CC $CFLAGS -o +x/khtpm_hq_manager.+x khtpm_hq_manager.c

echo "OK +x/khtpm_hq_manager.+x"
