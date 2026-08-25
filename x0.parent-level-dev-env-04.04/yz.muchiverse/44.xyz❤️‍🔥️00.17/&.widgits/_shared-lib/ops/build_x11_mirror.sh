#!/bin/sh
# build_x11_mirror.sh — build the real, shared x11_mirror binary
# (2026-08-17, khtpm-merge-how2.md §5c.6, legacy-shared-fix.md §3).
# ONE compiled binary, launched by every legacy-GL project
# (mutaclysm/piececraft-xyz/my-chara-txt so far) in place of each
# project's own separate gl_mirror.c copy - see x11_mirror.c's own
# header comment for the real parameterization approach (window title
# derived from basename(project_root), no new config needed).
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"
LIBS="-I/usr/include/freetype2 -lX11 -lXft"

echo "-- x11_mirror -> +x/x11_mirror.+x"
$CC $CFLAGS -o +x/x11_mirror.+x x11_mirror.c $LIBS
echo "OK +x/x11_mirror.+x"
