#!/bin/sh
# build_desk_synch.sh - builds desk_synch into +x/ (Synch between a game's book/rooms and the LyfeDesk's open book/page).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/desk_synch.+x desk_synch.c
echo "OK +x/desk_synch.+x"
