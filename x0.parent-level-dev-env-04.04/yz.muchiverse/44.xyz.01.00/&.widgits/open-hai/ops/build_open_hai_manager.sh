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

# REAL, NEW 2026-09-29 - generic dot-notation JSON parser, ported from
# gem-dev (see json_parser.c's own header). The manager forks/execs
# this as a real, separate binary (not a static-linked function) so a
# raw response's parsing can't crash the long-running manager process.
echo "-- json parser -> +x/json_parser.+x"
$CC $CFLAGS -o +x/json_parser.+x json_parser.c

echo "OK +x/json_parser.+x"
