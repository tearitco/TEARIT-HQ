#!/bin/bash
# scripts/build.sh - compile everything, warning-free where possible.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: system/*.c here are
# real local copies of @.apps/my-chara-txt's own proven system/ sources.
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
if gcc $CFLAGS -o "system/gl_mirror" "system/gl_mirror.c" -lglut -lGL -lGLU -lX11 2>/tmp/tactics_gl_mirror_build.log; then
    echo "    gl_mirror: built ok"
else
    echo "    gl_mirror: skipped (GLUT/GL not available - see /tmp/tactics_gl_mirror_build.log)"
    rm -f /tmp/tactics_gl_mirror_build.log
fi

echo "--- Building tactics-txt ops ---"
gcc $CFLAGS -o "ops/+x/tactics_menu_input.+x" "ops/tactics_menu_input.c"
gcc $CFLAGS -o "ops/+x/tactics_compose_frame.+x" "ops/tactics_compose_frame.c"

echo "build ok"
