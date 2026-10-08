#!/bin/sh
# build_eden_viewer_manager.sh - compile the eden-viewer-hq window backend.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 -o "$HERE/+x/eden_viewer_manager.+x" "$HERE/eden_viewer_manager.c"
echo "OK $HERE/+x/eden_viewer_manager.+x"
