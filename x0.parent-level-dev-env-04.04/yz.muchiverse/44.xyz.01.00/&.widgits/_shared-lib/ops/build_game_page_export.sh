#!/bin/sh
# build_game_page_export.sh - builds game_page_export into +x/ (writes a BOOK:PAGE page file for a game from its live state).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/game_page_export.+x game_page_export.c
echo "OK +x/game_page_export.+x"
