#!/bin/sh
# build_chtpm_rgb_render.sh — the real, canonical chtpm_rgb_render.c
# source (2026-08-17, khtpm-merge-how2.md §5c.7, legacy-shared-fix.md
# §2). This is a SOURCE consolidation, not a shared-binary one: the 9
# real projects that had a byte-identical copy now symlink their own
# system/chtpm_rgb_render.c here and still build/launch their OWN local
# binary as before - only the .c file is shared, no launcher changes
# needed. This build script exists for reference/standalone compile
# checks only; each project's own scripts/build.sh or button.sh still
# does the real per-project compile.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- chtpm_rgb_render (canonical, symlinked by 9 real projects) -> +x/chtpm_rgb_render.+x"
$CC $CFLAGS -o +x/chtpm_rgb_render.+x chtpm_rgb_render.c
echo "OK +x/chtpm_rgb_render.+x"
