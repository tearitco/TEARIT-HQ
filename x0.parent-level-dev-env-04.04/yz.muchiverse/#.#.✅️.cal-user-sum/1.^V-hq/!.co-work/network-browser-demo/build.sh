#!/bin/bash
# build.sh - builds this demo's two real pieces:
#   khtpm_generic_host.+x            - the generic renderer (this demo's own new code)
#   &.hq-apps/network/+x/network_browser_manager.+x - the real, unmodified house manager
set -eu
SDIR="$(cd "$(dirname "$0")" && pwd)"
CC=${CC:-gcc}

SHARED="$(cd "$SDIR/../khtpm-core" && pwd)"
cp "$SHARED/khtpm_css_parser.c" "$SDIR/khtpm_css_parser.c"
cp "$SHARED/khtpm_css_parser.h" "$SDIR/khtpm_css_parser.h"
cp "$SHARED/khtpm_render_core.c" "$SDIR/khtpm_render_core.c"
cp "$SHARED/khtpm_chtpm_loader.c" "$SDIR/khtpm_chtpm_loader.c"

CFLAGS="-std=c11 -Wall -O2 $(pkg-config --cflags xft)"
LIBS="-lX11 $(pkg-config --libs xft) -lm"

echo "-- khtpm_generic_host -> khtpm_generic_host.+x"
$CC $CFLAGS -o "$SDIR/khtpm_generic_host.+x" "$SDIR/khtpm_generic_host.c" "$SDIR/khtpm_css_parser.c" $LIBS

mkdir -p "$SDIR/&.hq-apps/network/+x"
echo "-- network_browser_manager (real, unmodified house code) -> &.hq-apps/network/+x/network_browser_manager.+x"
$CC -std=c11 -Wall -O2 -o "$SDIR/&.hq-apps/network/+x/network_browser_manager.+x" "$SDIR/&.hq-apps/network/network_browser_manager.c"

echo "OK"
