#!/bin/sh
# build_ui_sound_render.sh - build ops/+x/ui_sound_render.+x (U1 of the UI sounds design). Run from anywhere.
HERE="$(cd "$(dirname "$0")" && pwd)"; SHARED="$HERE/../../_shared-lib"
mkdir -p "$HERE/+x" && gcc -std=gnu11 -Wall -Wextra -Werror -O2 -isystem "$SHARED" -o "$HERE/+x/ui_sound_render.+x" "$HERE/ui_sound_render.c" -lm && echo "OK $HERE/+x/ui_sound_render.+x"
