#!/bin/bash
# build.sh - builds this demo's two real pieces:
#   *.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x - the REAL,
#     complete, unmodified shared house renderer (see that folder's own
#     build_core_render.sh for what's included/left out).
#   &.hq-apps/network/+x/network_browser_manager.+x - the real,
#     unmodified house manager (fetch + simple HTML extraction).
set -eu
SDIR="$(cd "$(dirname "$0")" && pwd)"
CC=${CC:-gcc}

sh "$SDIR/*.monads/*.livedesk-taskbar/ops/build_core_render.sh"

mkdir -p "$SDIR/&.hq-apps/network/+x"
echo "-- network_browser_manager (real, unmodified house code) -> &.hq-apps/network/+x/network_browser_manager.+x"
$CC -std=c11 -Wall -O2 -o "$SDIR/&.hq-apps/network/+x/network_browser_manager.+x" "$SDIR/&.hq-apps/network/network_browser_manager.c"

echo "OK"
