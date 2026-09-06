#!/bin/sh
# build_canvascraft_manager.sh - compile the Canvas-Craft window backend.
# Mirrors &.hq-apps/irc-chat-hq/ops/build_irc_chat_manager.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/canvascraft_manager.+x" "$HERE/canvascraft_manager.c"
echo "OK $HERE/+x/canvascraft_manager.+x"
