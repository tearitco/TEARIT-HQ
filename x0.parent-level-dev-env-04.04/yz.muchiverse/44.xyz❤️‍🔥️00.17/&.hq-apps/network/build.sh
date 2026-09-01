#!/bin/bash
# build.sh - builds the network cell's real apps.
#   cli_io_window.+x            - old cli-io stub (still built, unchanged - not retired here)
#   network_browser_manager.+x  - CENTROID_GOLD_STD.md proof: real manager (fetch + simple HTML extraction)
#   network_browser_render.+x   - CENTROID_GOLD_STD.md proof: real X11/khtpm Elem-tree renderer
#   network_browser_render_ascii.+x - CENTROID_GOLD_STD.md proof: real CLI/headless mirror, same manager state
# Binaries live in +x/ (git-ignored).
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$SDIR/+x"
CC=${CC:-gcc}

echo "-- cli_io_window -> +x/cli_io_window.+x"
$CC -O2 -Wall -Wno-unused-result -o "$SDIR/+x/cli_io_window.+x" "$SDIR/cli_io_window.c" -lX11 && echo "OK cli_io_window" || exit 1

echo "-- network_browser_manager -> +x/network_browser_manager.+x"
$CC -std=c11 -Wall -O2 -o "$SDIR/+x/network_browser_manager.+x" "$SDIR/network_browser_manager.c" && echo "OK network_browser_manager" || exit 1

# real shared-source convention (khtpm-merge-how2.md's own "HOUSE
# STANDARD" - copy, don't hand-fork) - same shape as build_core_render.sh
SHARED="$(cd "$SDIR/../../&.widgits/_shared-lib" && pwd)"
cp "$SHARED/khtpm_css_parser.c" "$SDIR/khtpm_css_parser.c"
cp "$SHARED/khtpm_css_parser.h" "$SDIR/khtpm_css_parser.h"
cp "$SHARED/khtpm_render_core.c" "$SDIR/khtpm_render_core.c"

if [ "$(uname -s)" = "Darwin" ]; then
    PKG_CONFIG_PATH="/opt/X11/lib/pkgconfig:/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    export PKG_CONFIG_PATH
    X11_FLAGS="-I/opt/X11/include -L/opt/X11/lib"
else
    X11_FLAGS=""
fi
CFLAGS="-std=c11 -Wall -O2 $(pkg-config --cflags xft)"
LIBS="-lX11 $(pkg-config --libs xft) -lm"

echo "-- network_browser_render (X11) -> +x/network_browser_render.+x"
$CC $CFLAGS $X11_FLAGS -o "$SDIR/+x/network_browser_render.+x" \
  "$SDIR/network_browser_render.c" "$SDIR/khtpm_css_parser.c" $LIBS && echo "OK network_browser_render" || exit 1

echo "-- network_browser_render_ascii (CLI) -> +x/network_browser_render_ascii.+x"
$CC -std=c11 -Wall -O2 -o "$SDIR/+x/network_browser_render_ascii.+x" "$SDIR/network_browser_render_ascii.c" && echo "OK network_browser_render_ascii" || exit 1
