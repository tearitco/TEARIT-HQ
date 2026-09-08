#!/bin/sh
# build_screen_rec_manager.sh - <module> behind screen-rec-hq.xhtpm
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/screen_rec_manager.+x" "$HERE/screen_rec_manager.c"
echo "OK $HERE/+x/screen_rec_manager.+x"
