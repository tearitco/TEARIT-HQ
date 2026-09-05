#!/bin/sh
# build_file_explorer_manager.sh - compile the file-explorer widget's
# directory-scanning manager. Mirrors taskbar-settings/ops/
# build_taskbar_settings_projector.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/file_explorer_manager.+x" "$HERE/file_explorer_manager.c"
echo "OK $HERE/+x/file_explorer_manager.+x"
