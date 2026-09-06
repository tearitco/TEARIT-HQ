#!/bin/sh
# build_music_player_manager.sh - compile music-player-hq's backend
# (library scan + mpg123 -R child + state publish). Mirrors
# pdl-read/ops/build_pdl_read_manager.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/music_player_manager.+x" "$HERE/music_player_manager.c"
echo "OK $HERE/+x/music_player_manager.+x"
