#!/bin/bash
# scripts/build.sh - compile everything, warning-free where possible.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: system/*.c here are
# real local copies of @.apps/tactics-txt's own proven system/ sources
# (same file-menu/tactics-txt/civ-txt-established convention).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

# Resolve shared-lib canonical source (symlink-free)
_sr="$PWD"; while [ ! -d "$_sr/&.widgits/_shared-lib" ] && [ "$_sr" != "/" ]; do _sr="$(dirname "$_sr")"; done
_SS="$_sr/&.widgits/_shared-lib"

mkdir -p ops/+x system

CFLAGS="-Wall -Wextra -O2"

echo "--- Building system processes ---"
gcc $CFLAGS "$_SS/system/prisc+x.c" -o "system/prisc+x"
gcc $CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
gcc $CFLAGS "system/renderer.c" -o "system/renderer"

echo "--- Building chtpm_parser_pal (PERSISTENT process, -Wno-unused-result"
echo "    -Wno-stringop-truncation required - see chtpm_parser_pal.c) ---"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "$_SS/system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

echo "--- Building chtpm_rgb_render (local copy, PERSISTENT daemon) ---"
gcc $CFLAGS "$_SS/ops/chtpm_rgb_render.c" -o "system/chtpm_rgb_render"

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

echo "--- Building piececraft-hq ops ---"
gcc $CFLAGS -o "ops/+x/pc_menu_input.+x" "ops/pc_menu_input.c"
gcc $CFLAGS -o "ops/+x/pc_compose_frame.+x" "ops/pc_compose_frame.c"
gcc $CFLAGS -o "ops/+x/pc_generate_chunk.+x" "ops/pc_generate_chunk.c"
gcc $CFLAGS -o "ops/+x/pc_phymoji_gen.+x" "ops/pc_phymoji_gen.c" -lm
gcc $CFLAGS -o "ops/+x/pc_clock_daemon.+x" "ops/pc_clock_daemon.c" -lm
gcc $CFLAGS -o "ops/+x/pc_world_manager.+x" "ops/pc_world_manager.c"
gcc $CFLAGS -o "ops/+x/pc_hq_status_manager.+x" "ops/pc_hq_status_manager.c"

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
WSR="$(cd "$SCRIPT_DIR/../.." && pwd)/014.wsr-pal💸️📌️+2"
if [ -x "$WSR/ops/+x/emoji_gen_atlas.+x" ]; then
    cp "$WSR/ops/+x/emoji_gen_atlas.+x" "ops/+x/emoji_gen_atlas.+x"
    chmod +x "ops/+x/emoji_gen_atlas.+x"
    echo "    emoji_gen_atlas.+x copied ok"
else
    echo "    WARN: emoji_gen_atlas.+x not found at $WSR - pc_phymoji_gen.+x will fail until it exists"
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
