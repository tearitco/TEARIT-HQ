#!/bin/sh
# build_dsr_manager.sh - build DSR's own real manager, same -o +x/<name>.+x
# pattern every other real manager in this house uses.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2 -D_POSIX_C_SOURCE=199309L"
echo "-- dsr_manager.c -> +x/dsr_manager.+x"
$CC $CFLAGS -o +x/dsr_manager.+x dsr_manager.c
echo "OK +x/dsr_manager.+x"
