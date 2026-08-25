#!/bin/bash
# scripts/build.sh - compile everything, warning-free where possible.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: system/*.c here are
# real local copies of @.apps/tactics-txt's own proven system/ sources
# (same file-menu/tactics-txt/civ-txt-established convention).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p ops/+x system

CFLAGS="-Wall -Wextra -O2"

echo "--- Building system processes ---"
gcc $CFLAGS "system/prisc+x.c" -o "system/prisc+x"
gcc $CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
gcc $CFLAGS "system/renderer.c" -o "system/renderer"

echo "--- Building chtpm_parser_pal (PERSISTENT process, -Wno-unused-result"
echo "    -Wno-stringop-truncation required - see chtpm_parser_pal.c) ---"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

echo "--- Building chtpm_rgb_render (local copy, PERSISTENT daemon) ---"
gcc $CFLAGS "system/chtpm_rgb_render.c" -o "system/chtpm_rgb_render"

echo "--- Building orchestrator ---"
gcc $CFLAGS -o "system/orchestrator" "system/orchestrator.c"

echo "--- Building gl_mirror (optional GL/GLUT reader, skips gracefully"
echo "    if GLUT/GL dev libs aren't available) ---"
if gcc $CFLAGS -o "system/gl_mirror" "system/gl_mirror.c" -lglut -lGL -lGLU -lX11 2>/tmp/piececraft_gl_mirror_build.log; then
    echo "    gl_mirror: built ok"
else
    echo "    gl_mirror: skipped (GLUT/GL not available - see /tmp/piececraft_gl_mirror_build.log)"
    rm -f /tmp/piececraft_gl_mirror_build.log
fi

echo "--- Building mutaclysm ops ---"
gcc $CFLAGS -o "ops/+x/mua_menu_input.+x" "ops/mua_menu_input.c"
gcc $CFLAGS -o "ops/+x/mua_compose_frame.+x" "ops/mua_compose_frame.c"
gcc $CFLAGS -o "ops/+x/mua_generate_chunk.+x" "ops/mua_generate_chunk.c"
gcc $CFLAGS -o "ops/+x/mua_phymoji_gen.+x" "ops/mua_phymoji_gen.c" -lm
gcc $CFLAGS -o "ops/+x/mua_clock_daemon.+x" "ops/mua_clock_daemon.c" -lm

echo "--- Building 3D ops (stolen from board-viewer) ---"
gcc $CFLAGS -fopenmp "ops/muta_render_3d.c" -o "ops/+x/muta_render_3d.+x" -lm
gcc $CFLAGS -fopenmp "ops/muta_compose_frame_3d.c" -o "ops/+x/muta_compose_frame_3d.+x" -lm
gcc $CFLAGS -fopenmp "ops/muta_menu_input_3d.c" -o "ops/+x/muta_menu_input_3d.+x" -lm

echo "--- Building classic game ops (restored from +18.0G) ---"
gcc $CFLAGS -o "ops/+x/game_dispatch.+x" "ops/game_dispatch.c"
gcc $CFLAGS -o "ops/+x/camera_control.+x" "ops/camera_control.c"
gcc $CFLAGS -o "ops/+x/move_player.+x" "ops/move_player.c"
gcc $CFLAGS -o "ops/+x/choice.+x" "ops/choice.c"
gcc $CFLAGS -o "ops/+x/end_turn.+x" "ops/end_turn.c"
gcc $CFLAGS -o "ops/+x/tick_monsters.+x" "ops/tick_monsters.c"
gcc $CFLAGS -o "ops/+x/pickup.+x" "ops/pickup.c"
gcc $CFLAGS -o "ops/+x/drop.+x" "ops/drop.c"
gcc $CFLAGS -o "ops/+x/eat.+x" "ops/eat.c"
gcc $CFLAGS -o "ops/+x/craft.+x" "ops/craft.c"
gcc $CFLAGS -o "ops/+x/examine.+x" "ops/examine.c"
gcc $CFLAGS -o "ops/+x/save_game.+x" "ops/save_game.c"
gcc $CFLAGS -o "ops/+x/toggle_emoji.+x" "ops/toggle_emoji.c"
gcc $CFLAGS -o "ops/+x/compose_frame.+x" "ops/compose_frame.c"
gcc $CFLAGS -o "ops/+x/compose_rgb_frame.+x" "ops/compose_rgb_frame.c" -lm
gcc $CFLAGS -I"ops" -o "ops/+x/dump_rgb_png.+x" "ops/dump_rgb_png.c" -lm
gcc $CFLAGS -o "ops/+x/pdl_reader.+x" "ops/pdl_reader.c"
gcc $CFLAGS -o "ops/+x/title_input.+x" "ops/title_input.c"
gcc $CFLAGS -o "ops/+x/compose_title_frame.+x" "ops/compose_title_frame.c"
gcc $CFLAGS -o "ops/+x/generate_map.+x" "ops/generate_map.c"
gcc $CFLAGS -o "ops/+x/muta_widget_cmds.+x" "ops/muta_widget_cmds.c"
gcc $CFLAGS -o "ops/+x/convert_og_map_to_voxels.+x" "ops/convert_og_map_to_voxels.c"

echo "--- Copying real emoji_gen_atlas.+x + emoji_xtract.+x (real FreeType"
echo "    rasterizer + real box-filter/crop extractor - board-viewer's own"
echo "    build.sh copies these SAME real binaries from wsr-pal, not"
echo "    reinvented here). REAL FIX 2026-08-04, direct user report ('2d"
echo "    emoji mode is all grey, not the emojis that use to show'): this"
echo "    project's own system/chtpm_rgb_render.c was a genuinely STALE"
echo "    local copy (Aug 3) that predates wsr-pal's own real 'GENERIC"
echo "    ON-DEMAND EMOJI GENERATION' feature (Jul 30, but a NEWER real"
echo "    source than piececraft's own copy had) - that feature needs"
echo "    BOTH binaries below present in THIS project's own ops/+x/ to"
echo "    actually generate a tile's real emoji texture; missing"
echo "    emoji_xtract.+x meant every non-hand-curated tile (grass/dirt/"
echo "    trees) silently fell back to a flat grey placeholder. Refreshed"
echo "    system/chtpm_rgb_render.c from wsr-pal's own newer copy too (a"
echo "    plain, uncustomized file here - safe to refresh wholesale, see"
echo "    this same session's own real diff check before copying) ---"
# REAL BUG FIX 2026-08-18: this used to be "$SCRIPT_DIR/../.." (two levels
# up) - SCRIPT_DIR is already this PROJECT'S OWN ROOT (build.sh does
# `cd "$(dirname "$0")/.." && pwd` at its own top), and wsr-pal is a DIRECT
# SIBLING (both live inside 44.xyz.../ together) - one level up, not two.
# The old path silently resolved past 44.xyz.../ entirely (landing in
# yz.muchiverse/ instead), so `-x "$WSR/ops/+x/emoji_gen_atlas.+x"` always
# failed and every single build this whole session printed the
# "WARN: emoji_gen_atlas.+x not found" line below - this is the real root
# cause of "emoji texture wrapping for walls" never rendering; the voxel
# renderer's own on-demand emoji generation needs both binaries copied in
# by THIS block to produce anything but a flat grey/solid-color fallback.
WSR="$(cd "$SCRIPT_DIR/.." && pwd)/014.wsr-pal💸️📌️+2"
if [ -x "$WSR/ops/+x/emoji_gen_atlas.+x" ]; then
    cp "$WSR/ops/+x/emoji_gen_atlas.+x" "ops/+x/emoji_gen_atlas.+x"
    chmod +x "ops/+x/emoji_gen_atlas.+x"
    echo "    emoji_gen_atlas.+x copied ok"
else
    echo "    WARN: emoji_gen_atlas.+x not found at $WSR - mua_phymoji_gen.+x will fail until it exists"
fi
if [ -x "$WSR/ops/+x/emoji_xtract.+x" ]; then
    cp "$WSR/ops/+x/emoji_xtract.+x" "ops/+x/emoji_xtract.+x"
    chmod +x "ops/+x/emoji_xtract.+x"
    echo "    emoji_xtract.+x copied ok"
else
    echo "    WARN: emoji_xtract.+x not found at $WSR - 2D on-demand emoji generation will fall back to grey"
fi
mkdir -p pieces/registry/emoji_assets

echo "build ok"
