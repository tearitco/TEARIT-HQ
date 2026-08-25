#!/bin/sh
# Builds every binary independently - no shared object files, no shared
# headers, matching this project family's own convention (see
# mutaclsym/dox/00-HANDOFF.md's own build.sh for the sibling reference
# this was copied in shape from).
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: this project keeps its
# own real, local copy of every file below that also exists in
# yz.muchiverse/2.muchi-verse/shared-ops/ (system/prisc+x.c,
# system/keyboard_input.c, system/chtpm_parser_pal.c,
# system/chtpm_rgb_render.c, ops/xlector_input.c, ops/move_entity.c,
# ops/pdl_reader.c, ops/pet_export.c, ops/pet_import.c,
# ops/dump_rgb_png.c, ops/lib/) - direct user instruction: "i dont wanna
# use shared ops, in the classic sense... all code should be self
# independant and solo shippable." This build.sh never reaches outside
# this project's own directory. To pull in an update from the canonical
# source, run (from yz.muchiverse/2.muchi-verse/):
#   bash sync_shared_op.sh <op_name> <target_dir>
# (see that directory's own shared-ops-manifest.txt for available
# op_name values and this project's own consumer list) - a deliberate,
# explicit step, never automatic at build time.
set -e
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -Wextra -O2"

echo "-- prisc+x (VM)"
$CC $CFLAGS -o system/prisc+x system/prisc+x.c

echo "-- keyboard_input (raw termios, no ncurses - local copy, see"
echo "   system/keyboard_input.c's own header comment)"
$CC $CFLAGS -o system/keyboard_input system/keyboard_input.c

echo "-- renderer (plain stdout, no ncurses)"
$CC $CFLAGS -o system/renderer system/renderer.c

echo "-- chtpm_parser_pal (PERSISTENT process, not a one-shot op - see"
echo "   chtpm-to-pal-layout-plan.txt and that file's own header"
echo "   comment. -Wno-unused-result -Wno-stringop-truncation are"
echo "   REQUIRED on this one file - confirmed via a real test build"
echo "   this gets to zero warnings.)"
$CC $CFLAGS -Wno-unused-result -Wno-stringop-truncation -o system/chtpm_parser_pal system/chtpm_parser_pal.c

echo "-- chtpm_rgb_render (local copy, PERSISTENT daemon - real wraith_rgb_daemon.c"
echo "   equivalent: font-rasterizes pieces/display/current_frame.txt"
echo "   verbatim, zero .chtpm awareness - see that file's own header"
echo "   comment. Writes the SAME rgb_frame.raw/receipt gl_mirror already"
echo "   reads, sized to gl_mirror's own hardcoded 640x304 on purpose.)"
$CC $CFLAGS -o system/chtpm_rgb_render system/chtpm_rgb_render.c

echo "-- gl_mirror (optional GL/GLUT reader - mutaclsym-style: shows the"
echo "   LEVEL ONLY (map+xlector, no pets - see ops/compose_rgb_frame.c's"
echo "   own header comment), the only file allowed to call GL primitives)"
if $CC $CFLAGS -o system/gl_mirror system/gl_mirror.c -lglut -lGL -lGLU 2>/tmp/gl_mirror_build.log; then
    echo "   ok"
else
    echo "   skipped (GLUT/GL not available - see /tmp/gl_mirror_build.log)"
    rm -f /tmp/gl_mirror_build.log
fi

echo "-- zoo_window (optional GL/X11 desktop window per piece - real"
echo "   drag-to-grid-snap, ported from egg-pals' own system/egg_window.c"
echo "   - see dox/pet-import-export-standard.md for the port notes. NOT"
echo "   launched by 'run' - that's now z0.egg-pals+13's job; kept here"
echo "   only for manual './button.sh window <id>' testing.)"
if $CC $CFLAGS -o system/zoo_window system/zoo_window.c -lX11 -lXext -lGL -lGLU -lm 2>/tmp/zoo_window_build.log; then
    echo "   ok"
else
    echo "   skipped (X11/GLX not available - see /tmp/zoo_window_build.log)"
    rm -f /tmp/zoo_window_build.log
fi

echo "-- local copies of formerly-shared ops (see this file's own header"
echo "   comment - sync_shared_op.sh propagates canonical updates here)"
$CC $CFLAGS -o ops/+x/xlector_input.+x ops/xlector_input.c
$CC $CFLAGS -o ops/+x/move_entity.+x ops/move_entity.c
$CC $CFLAGS -o ops/+x/pdl_reader.+x ops/pdl_reader.c
$CC $CFLAGS -o ops/+x/pet_export.+x ops/pet_export.c
$CC $CFLAGS -o ops/+x/pet_import.+x ops/pet_import.c
echo "-- dump_rgb_png (local copy - DEBUG TOOL, not wired into pal/"
echo "   main_loop.pal or default_op.txt - run manually to see the GL"
echo "   mirror's actual pixels as a real PNG)"
$CC $CFLAGS -I"ops/lib" -o ops/+x/dump_rgb_png.+x ops/dump_rgb_png.c
echo "-- palnet_peer (local copy - reusable P2P companion process, see"
echo "   PAL-NET-STANDARD.txt - launched by button.sh alongside"
echo "   gl_mirror, not compiled into any GUI process directly)"
$CC $CFLAGS -o ops/+x/palnet_peer.+x ops/palnet_peer.c

echo "-- zoo_0000-specific ops"
$CC $CFLAGS -o ops/+x/tick_pets.+x ops/tick_pets.c
$CC $CFLAGS -o ops/+x/compose_frame.+x ops/compose_frame.c
$CC $CFLAGS -o ops/+x/compose_rgb_frame.+x ops/compose_rgb_frame.c
$CC $CFLAGS -o ops/+x/feed_pet.+x ops/feed_pet.c
$CC $CFLAGS -o ops/+x/pet_pet.+x ops/pet_pet.c
$CC $CFLAGS -o ops/+x/play_pet.+x ops/play_pet.c

echo "build ok"
