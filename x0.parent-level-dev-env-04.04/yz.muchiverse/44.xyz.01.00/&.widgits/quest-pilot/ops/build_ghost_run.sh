#!/bin/sh
# build_ghost_run.sh - compile ghost_run (self-contained C, no shared headers; fork+exec only).
# -Wno-unknown-warning-option comes first so clang on macOS ignores the gcc-only -Wno-* after it.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
$CC -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-format-truncation -O2 -o +x/ghost_run.+x ghost_run.c
# ghost_run runs quest_check.+x from the SAME +x directory (precheck + scope/base check); build it here so a clean rebuild of this folder can never leave ghost_run unable to run.
sh ./build_quest_check.sh >/dev/null
[ -x +x/quest_check.+x ] || { echo "FAIL: +x/quest_check.+x was not built"; exit 1; }
echo "OK +x/ghost_run.+x and +x/quest_check.+x"
