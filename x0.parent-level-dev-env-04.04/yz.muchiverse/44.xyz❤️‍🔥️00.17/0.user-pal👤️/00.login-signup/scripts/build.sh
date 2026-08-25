#!/bin/bash
# scripts/build.sh - compile everything, warning-free.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: this project keeps its
# own real, local copy of every file below that also exists in
# yz.muchiverse/2.muchi-verse/shared-ops/ (system/prisc+x.c,
# system/keyboard_input.c, system/chtpm_parser_pal.c, system/renderer.c)
# - same convention every other project in this family follows (see
# ../shared-ops-manifest.txt). To pull in an update from the canonical
# source, run (from yz.muchiverse/2.muchi-verse/):
#   bash sync_shared_op.sh <op_name> user-pal/<target_dir>
#
# No chtpm_rgb_render, no palnet_peer - user-pal is purely local (no
# networking, no RGB mode needed for a single login screen).
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
echo "    -Wno-stringop-truncation required - see shared-ops/chtpm_parser_pal.c)"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

echo "--- Building user-pal ops ---"
gcc $CFLAGS -o "ops/+x/userpal_create_account.+x" "ops/userpal_create_account.c"
gcc $CFLAGS -o "ops/+x/userpal_login.+x" "ops/userpal_login.c"
gcc $CFLAGS -o "ops/+x/userpal_logout.+x" "ops/userpal_logout.c"
gcc $CFLAGS -o "ops/+x/userpal_whoami.+x" "ops/userpal_whoami.c"
gcc $CFLAGS -o "ops/+x/userpal_menu_input.+x" "ops/userpal_menu_input.c"
gcc $CFLAGS -o "ops/+x/userpal_compose_frame.+x" "ops/userpal_compose_frame.c"

echo "build ok"
