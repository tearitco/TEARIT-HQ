#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"
mkdir -p ops/+x system
CFLAGS="-Wall -Wextra -O2"
echo "--- system ---"
gcc $CFLAGS "system/prisc+x.c" -o "system/prisc+x"
gcc $CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
gcc $CFLAGS "system/renderer.c" -o "system/renderer"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"
echo "--- ops ---"
gcc $CFLAGS -o "ops/+x/start_scan.+x" "ops/start_scan.c"
gcc $CFLAGS -o "ops/+x/start_compose_frame.+x" "ops/start_compose_frame.c"
gcc $CFLAGS -o "ops/+x/start_menu_input.+x" "ops/start_menu_input.c"
echo "build ok"
