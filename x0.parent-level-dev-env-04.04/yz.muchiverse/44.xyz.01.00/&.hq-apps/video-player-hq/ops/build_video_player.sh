#!/bin/sh
# build_video_player.sh - compile the video-player-hq manager and its command op.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 -o "$HERE/+x/video_player_manager.+x" "$HERE/video_player_manager.c"
gcc -std=c11 -Wall -Wextra -O2 -o "$HERE/+x/video_player_op.+x" "$HERE/video_player_op.c"
echo "OK $HERE/+x"
