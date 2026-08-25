#!/bin/bash
# scripts/build.sh - compile TSC_ELO system processes + ops, warning-free.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: system/*.c here are real
# local copies of 041.pal-chain's own proven system/ sources (same
# convention every project in this family follows - see @.apps/my-chara-txt
# scripts/build.sh and ../shared-ops-manifest.txt for the precedent).
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

echo "--- Building orchestrator (local copy, -Wno-unused-result) ---"
gcc $CFLAGS -Wno-unused-result -o "system/orchestrator" "system/orchestrator.c"

echo "--- Building gl_mirror (optional GL/GLUT reader, skips gracefully"
echo "    if GLUT/GL dev libs aren't available) ---"
if gcc $CFLAGS -o "system/gl_mirror" "system/gl_mirror.c" -lglut -lGL -lGLU -lX11 2>/tmp/tsc_gl_mirror_build.log; then
    echo "    gl_mirror: built ok"
else
    echo "    gl_mirror: skipped (GLUT/GL not available - see /tmp/tsc_gl_mirror_build.log)"
    rm -f /tmp/tsc_gl_mirror_build.log
fi

echo "--- Building TSC_ELO ops ---"
gcc $CFLAGS -o "ops/+x/tsc_compose.+x" "ops/tsc_compose.c"
gcc $CFLAGS -o "ops/+x/tsc_tick.+x" "ops/tsc_tick.c"
gcc $CFLAGS -o "ops/+x/tsc_setup.+x" "ops/tsc_setup.c"
gcc $CFLAGS -o "ops/+x/tsc_elo.+x" "ops/tsc_elo.c" -lm
gcc $CFLAGS -o "ops/+x/tsc_ai.+x" "ops/tsc_ai.c"
gcc $CFLAGS -o "ops/+x/tsc_deal.+x" "ops/tsc_deal.c"
gcc $CFLAGS -o "ops/+x/tsc_input.+x" "ops/tsc_input.c"
gcc $CFLAGS -o "ops/+x/tsc_net.+x" "ops/tsc_net.c"
gcc $CFLAGS -o "ops/+x/tsc_miracle.+x" "ops/tsc_miracle.c"
gcc $CFLAGS -o "ops/+x/ledger_append.+x" "ops/ledger_append.c"
gcc $CFLAGS -o "ops/+x/ledger_peers.+x" "ops/ledger_peers.c"

echo "--- Building SETUP WIDGIT (W1, own program) ---"
bash "$SCRIPT_DIR/widgets/setup/scripts/build.sh"

echo "build ok"
