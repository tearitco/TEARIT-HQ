#!/bin/bash
# scripts/build.sh - compile everything, warning-free.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: this project keeps its
# own real, local copy of every file below that also exists in
# yz.muchiverse/2.muchi-verse/shared-ops/ (system/prisc+x.c,
# system/chtpm_parser_pal.c, system/chtpm_rgb_render.c,
# ops/dump_rgb_png.c, ops/lib/) - direct user instruction: "i dont wanna
# use shared ops, in the classic sense... all code should be self
# independant and solo shippable." system/keyboard_input.c is this
# project's OWN local fork, never synced from shared-ops/ at all (see
# ../shared-ops-manifest.txt's own CONSUMERS note). This build.sh never
# reaches outside this project's own directory. To pull in an update
# from the canonical source, run (from yz.muchiverse/2.muchi-verse/):
#   bash sync_shared_op.sh <op_name> <target_dir>
# (see that directory's own shared-ops-manifest.txt for available
# op_name values) - a deliberate, explicit step, never automatic at
# build time.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p ops/+x

CFLAGS="-Wall -Wextra -O2 -I/usr/include/freetype2"

echo "--- Building system processes ---"
gcc $CFLAGS "system/prisc+x.c" -o "system/prisc+x"
gcc $CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
gcc $CFLAGS "system/renderer.c" -o "system/renderer"

echo "--- Building chtpm_parser_pal (PERSISTENT process, not a"
echo "    one-shot op - see chtpm-to-pal-layout-plan.txt and that"
echo "    file's own header comment. -Wno-unused-result"
echo "    -Wno-stringop-truncation are REQUIRED on this one file -"
echo "    confirmed via a real test build this gets to zero warnings.)"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

echo "--- Building chtpm_rgb_render (local copy, PERSISTENT daemon - real"
echo "    wraith_rgb_daemon.c equivalent: font-rasterizes"
echo "    pieces/display/current_frame.txt verbatim, zero .chtpm"
echo "    awareness - see that file's own header comment. Writes the"
echo "    SAME rgb_frame.raw/receipt gl_mirror already reads.)"
gcc $CFLAGS "system/chtpm_rgb_render.c" -o "system/chtpm_rgb_render"

echo "--- Building gl_mirror (optional GL/GLUT reader - only file"
echo "    allowed to call GL primitives, see GOVERNING CONSTRAINT in"
echo "    2.muchi-verse/GRAND-ARCHITECTURE.md - built best-effort so a"
echo "    machine without GLUT dev headers/libs can still build the"
echo "    rest) ---"
if gcc $CFLAGS -o system/gl_mirror system/gl_mirror.c -lglut -lGL -lGLU 2>/tmp/wsr_gl_mirror_build.log; then
    echo "    ok"
else
    echo "    skipped (GLUT/GL not available - see /tmp/wsr_gl_mirror_build.log)"
    rm -f /tmp/wsr_gl_mirror_build.log
fi

echo "--- Building dump_rgb_png (local copy - DEBUG TOOL, not wired into"
echo "    pal/main_loop.pal or default_op.txt - run manually to see the"
echo "    RGB mirror's actual pixels as a real PNG) ---"
gcc $CFLAGS -I"ops/lib" -o ops/+x/dump_rgb_png.+x ops/dump_rgb_png.c -lm

echo "--- Building ops ---"
for src in ops/*.c; do
    name="$(basename "$src" .c)"
    [ "$name" = "dump_rgb_png" ] && continue
    echo "  Compiling $name..."
    gcc $CFLAGS "$src" -o "ops/+x/$name.+x" -lm -lfreetype
done

echo "--- Build Complete ---"
ls -l system/prisc+x ops/+x/
