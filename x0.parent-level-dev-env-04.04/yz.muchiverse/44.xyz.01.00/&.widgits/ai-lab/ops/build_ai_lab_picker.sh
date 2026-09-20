#!/bin/sh
# build_ai_lab_picker.sh - compile ai_lab_picker, the real "Add State"
# picker. Plain Xlib+Xft, same real deps every other khtpm window
# already links - no GLX (tile-picker's own precedent uses GL for a
# 3D desktop context this doesn't need).
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
$CC -std=c11 -Wall -Wextra -O2 $(pkg-config --cflags xft) \
  -o +x/ai_lab_picker.+x ai_lab_picker.c \
  -lX11 $(pkg-config --libs xft)
echo "OK +x/ai_lab_picker.+x"
