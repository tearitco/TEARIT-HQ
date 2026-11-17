#!/bin/sh
# build_emoji_icon_op.sh - build the shared emoji-to-app-icon op (emoji_icon_op.c; used by app-edit and the phone)
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/emoji_icon_op.+x emoji_icon_op.c && echo "OK +x/emoji_icon_op.+x"
