#!/bin/sh
# build_chat_hai_projector.sh - compile the chat-hai UI projector.
# Modelled on &.widgits/events-hq/ops/build_evhq_projector.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/chat_hai_projector.+x" "$HERE/chat_hai_projector.c"
echo "OK $HERE/+x/chat_hai_projector.+x"
