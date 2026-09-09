#!/bin/sh
# build_music_player_manager.sh - compile music-player-hq's backend
# (library scan + mpg123 -R child + state publish). Mirrors
# pdl-read/ops/build_pdl_read_manager.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
# -I "$SHARED": music_player_manager.c #includes kh_proc_registry.h
# (PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md §5 5b - track the mpg123 child).
_pcd="$HERE"; while [ "$_pcd" != "/" ] && [ ! -d "$_pcd/&.widgits/_shared-lib" ]; do _pcd="$(dirname "$_pcd")"; done
SHARED="$_pcd/&.widgits/_shared-lib"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 -I "$SHARED" \
    -o "$HERE/+x/music_player_manager.+x" "$HERE/music_player_manager.c"
echo "OK $HERE/+x/music_player_manager.+x"
