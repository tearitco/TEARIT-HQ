#!/bin/sh
# builds the shared window-placement op (2026-11-15). Needs Xlib (-lX11) only for the screen width.
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/window_place_op.+x window_place_op.c -lX11 && echo "OK +x/window_place_op.+x"
