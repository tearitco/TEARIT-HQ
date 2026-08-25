#!/bin/sh
# Builds every binary independently - no shared object files, no shared
# headers, matching cdda-tpm-std-fast.txt sec. 1. Each translation unit
# is compiled and linked on its own line.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: this project keeps its
# own real, local copy of every file below that also exists in
# yz.muchiverse/2.muchi-verse/shared-ops/ (system/prisc+x.c,
# system/keyboard_input.c, system/chtpm_parser_pal.c,
# system/chtpm_rgb_render.c, ops/pdl_reader.c, ops/dump_rgb_png.c,
# ops/lib/) - direct user instruction: "i dont wanna use shared ops, in
# the classic sense... all code should be self independant and solo
# shippable." This build.sh never reaches outside this project's own
# directory. To pull in an update from the canonical source, run (from
# yz.muchiverse/2.muchi-verse/):
#   bash sync_shared_op.sh <op_name> <target_dir>
# (see that directory's own shared-ops-manifest.txt for available
# op_name values and this project's own consumer list) - a deliberate,
# explicit step, never automatic at build time.
set -e
cd "$(dirname "$0")/.."

# Resolve shared-lib canonical source (symlink-free)
_sr="$PWD"; while [ ! -d "$_sr/&.widgits/_shared-lib" ] && [ "$_sr" != "/" ]; do _sr="$(dirname "$_sr")"; done
_SS="$_sr/&.widgits/_shared-lib"

CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -Wextra -O2"

echo "-- prisc+x (VM)"
$CC $CFLAGS -o system/prisc+x $_SS/system/prisc+x.c

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
$CC $CFLAGS -Wno-unused-result -Wno-stringop-truncation -o system/chtpm_parser_pal $_SS/system/chtpm_parser_pal.c

echo "-- chtpm_rgb_render (local copy, PERSISTENT daemon - real wraith_rgb_daemon.c"
echo "   equivalent: font-rasterizes pieces/display/current_frame.txt"
echo "   verbatim, zero .chtpm awareness - see that file's own header"
echo "   comment. Writes the SAME rgb_frame.raw/receipt gl_mirror already"
echo "   reads, sized to gl_mirror's own hardcoded 640x304 on purpose.)"
$CC $CFLAGS -o system/chtpm_rgb_render $_SS/ops/chtpm_rgb_render.c

echo "-- gl_mirror (optional GL/GLUT reader - only file allowed to call"
echo "   GL primitives, see GOVERNING CONSTRAINT in"
echo "   2.muchi-verse/GRAND-ARCHITECTURE.md - built best-effort so a"
echo "   machine without GLUT dev headers/libs can still build the rest)"
if $CC $CFLAGS -o system/gl_mirror system/gl_mirror.c -lglut -lGL -lGLU -lX11 2>/tmp/gl_mirror_build.log; then
    echo "   ok"
else
    echo "   skipped (GLUT/GL not available - see /tmp/gl_mirror_build.log)"
    rm -f /tmp/gl_mirror_build.log
fi

echo "-- ops"
$CC $CFLAGS -o ops/+x/move_player.+x ops/move_player.c
$CC $CFLAGS -o ops/+x/camera_control.+x ops/camera_control.c
$CC $CFLAGS -o ops/+x/end_turn.+x ops/end_turn.c
$CC $CFLAGS -o ops/+x/compose_frame.+x ops/compose_frame.c
$CC $CFLAGS -o ops/+x/compose_rgb_frame.+x ops/compose_rgb_frame.c -lm
echo "-- emoji_gen_atlas / emoji_xtract (ported from mutaclsym's own"
echo "   real fix 2026-07-31 - local copies, own source, NOT a shared-"
echo "   ops reference. This project's own compose_rgb_frame.c was"
echo "   missing ensure_emoji_asset_ready() entirely before this port -"
echo "   the real root cause of 3D GL mode showing flat colors instead"
echo "   of real emoji textures for any registry entry without a pre-"
echo "   baked voxels_16.csv on disk. See compose_rgb_frame.c's own"
echo "   ensure_emoji_asset_ready() header comment for the full story.)"
$CC $CFLAGS -I"ops/lib" -I/usr/include/freetype2 -o ops/+x/emoji_gen_atlas.+x ops/emoji_gen_atlas.c -lfreetype -lm
$CC $CFLAGS -I"ops/lib" -o ops/+x/emoji_xtract.+x ops/emoji_xtract.c -lm
echo "-- dump_rgb_png (local copy - DEBUG TOOL, not wired into pal/"
echo "   main_loop.pal or default_op.txt - run manually to see the RGB"
echo "   mirror's actual pixels as a real PNG, since this agent has no"
echo "   way to view the live GLUT window itself)"
$CC $CFLAGS -I"ops/lib" -o ops/+x/dump_rgb_png.+x ops/dump_rgb_png.c -lm
$CC $CFLAGS -o ops/+x/pickup.+x ops/pickup.c
$CC $CFLAGS -o ops/+x/drop.+x ops/drop.c
$CC $CFLAGS -o ops/+x/eat.+x ops/eat.c
$CC $CFLAGS -o ops/+x/tick_monsters.+x ops/tick_monsters.c
$CC $CFLAGS -o ops/+x/craft.+x ops/craft.c
$CC $CFLAGS -o ops/+x/examine.+x ops/examine.c
$CC $CFLAGS -o ops/+x/save_game.+x ops/save_game.c
$CC $CFLAGS -o ops/+x/toggle_emoji.+x ops/toggle_emoji.c
$CC $CFLAGS -o ops/+x/title_input.+x ops/title_input.c
$CC $CFLAGS -o ops/+x/compose_title_frame.+x ops/compose_title_frame.c
$CC $CFLAGS -o ops/+x/pdl_reader.+x ops/pdl_reader.c
$CC $CFLAGS -o ops/+x/choice.+x ops/choice.c
$CC $CFLAGS -o ops/+x/game_dispatch.+x ops/game_dispatch.c
$CC $CFLAGS -o ops/+x/generate_map.+x ops/generate_map.c

echo "build ok"
