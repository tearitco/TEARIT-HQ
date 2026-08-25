#!/bin/bash
# scripts/build.sh - compile everything, warning-free, matching
# mutaclsym's standing bar (see its dox/01-cdda-architecture.md §8).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p system ops/+x manager/+x

CFLAGS="-Wall -Wextra -O2"

echo "--- Building system processes ---"
gcc $CFLAGS "system/prisc+x.c" -o "system/prisc+x"
gcc $CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
gcc $CFLAGS "system/renderer.c" -o "system/renderer"

echo "--- Building chtpm_parser_pal (PERSISTENT process, -Wno-unused-result"
echo "    -Wno-stringop-truncation required - see shared-ops/chtpm_parser_pal.c) ---"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

echo "--- Building chtpm_rgb_render (PERSISTENT daemon - RGB mirror pipeline,"
echo "    font-rasterizes pieces/display/current_frame.txt into"
echo "    pieces/display/rgb_frame.raw; -Wno-format-truncation for one real"
echo "    gcc warning in the on-demand emoji csv_path snprintf) ---"
gcc $CFLAGS -Wno-format-truncation "system/chtpm_rgb_render.c" -o "system/chtpm_rgb_render"

echo "--- Building gl_mirror (optional GL/GLUT reader - only file allowed to"
echo "    call GL primitives; built best-effort so a machine without GLUT/GL"
echo "    can still build the rest) ---"
if gcc $CFLAGS -o system/gl_mirror system/gl_mirror.c -lglut -lGL -lGLU 2>/tmp/agent_gl_mirror_build.log; then
    echo "    ok"
else
    echo "    skipped (GLUT/GL not available - see /tmp/agent_gl_mirror_build.log)"
    rm -f /tmp/agent_gl_mirror_build.log
fi

echo "--- Building ops ---"
for src in ops/*.c; do
    name="$(basename "$src" .c)"
    case "$name" in
        emoji_gen_atlas|emoji_xtract) continue ;;
    esac
    echo "  Compiling $name..."
    gcc $CFLAGS "$src" -o "ops/+x/$name.+x"
done

echo "--- Building emoji ops (on-demand FreeType emoji generator used by"
echo "    chtpm_rgb_render's generic path - freetype headers required) ---"
gcc $CFLAGS -I/usr/include/freetype2 -o "ops/+x/emoji_gen_atlas.+x" "ops/emoji_gen_atlas.c" -lfreetype -lm
gcc $CFLAGS -o "ops/+x/emoji_xtract.+x" "ops/emoji_xtract.c" -lm

echo "--- Building manager (PERSISTENT process - path_nav_manager, see"
echo "    that file's own header comment) ---"
gcc $CFLAGS "manager/path_nav_manager.c" -o "manager/+x/path_nav_manager.+x"

echo "--- Build Complete ---"
ls -l system/prisc+x system/keyboard_input system/renderer system/chtpm_parser_pal ops/+x/ manager/+x/
