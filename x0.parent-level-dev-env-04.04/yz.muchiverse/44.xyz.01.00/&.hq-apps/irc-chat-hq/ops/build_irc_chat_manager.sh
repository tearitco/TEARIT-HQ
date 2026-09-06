#!/bin/sh
# build_irc_chat_manager.sh - compile the irc-chat-hq window backend.
# Mirrors @.apps/music-player-hq/ops/build_music_player_manager.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/irc_chat_manager.+x" "$HERE/irc_chat_manager.c"
echo "OK $HERE/+x/irc_chat_manager.+x"
