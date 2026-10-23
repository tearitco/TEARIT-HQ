#!/bin/sh
# build_ui_sound_player.sh - build ops/+x/ui_sound_player.+x (U2 of the UI sounds design). Needs libpulse-simple headers. Run from anywhere.
HERE="$(cd "$(dirname "$0")" && pwd)"; SHARED="$HERE/../../_shared-lib"
mkdir -p "$HERE/+x" && gcc -std=gnu11 -Wall -Wextra -Werror -O2 -isystem "$SHARED" -o "$HERE/+x/ui_sound_player.+x" "$HERE/ui_sound_player.c" -lpulse-simple -lpulse -lm && echo "OK $HERE/+x/ui_sound_player.+x"
