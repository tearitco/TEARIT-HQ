#!/bin/sh
# build_text_edit_manager.sh - compile text-edit-hq's own file/content
# manager. Mirrors pdl-read/ops/build_pdl_read_manager.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/text_edit_manager.+x" "$HERE/text_edit_manager.c"
echo "OK $HERE/+x/text_edit_manager.+x"
