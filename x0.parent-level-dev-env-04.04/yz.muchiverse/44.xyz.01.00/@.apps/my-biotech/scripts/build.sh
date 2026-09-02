#!/bin/bash
# scripts/build.sh - compile everything, warning-free where possible.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p ops/+x system

CFLAGS="-Wall -Wextra -O2"

echo "--- Building system processes ---"
gcc $CFLAGS "system/prisc+x.c" -o "system/prisc+x"
gcc $CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
gcc $CFLAGS "system/renderer.c" -o "system/renderer"

echo "--- Building chtpm_parser_pal ---"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

echo "--- Building chtpm_rgb_render ---"
gcc $CFLAGS "system/chtpm_rgb_render.c" -o "system/chtpm_rgb_render"

echo "--- Building orchestrator ---"
gcc $CFLAGS -o "system/orchestrator" "system/orchestrator.c"

echo "--- Building my-biotech ops ---"
gcc $CFLAGS -o "ops/+x/mybiotech_menu_input.+x" "ops/mybiotech_menu_input.c"
gcc $CFLAGS -o "ops/+x/mybiotech_compose_frame.+x" "ops/mybiotech_compose_frame.c"
gcc $CFLAGS -o "ops/+x/mybiotech_research_worker.+x" "ops/mybiotech_research_worker.c"
gcc $CFLAGS -o "ops/+x/mybiotech_fda_verdict.+x" "ops/mybiotech_fda_verdict.c"
gcc $CFLAGS -o "ops/+x/connect_op.+x" "ops/connect_op.c"
gcc $CFLAGS -o "ops/+x/json_parser.+x" "ops/json_parser.c"

echo "build ok"
