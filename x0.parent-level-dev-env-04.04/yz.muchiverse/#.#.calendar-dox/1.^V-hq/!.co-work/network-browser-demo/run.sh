#!/bin/bash
# run.sh - launch the demo standalone. house_root = this directory.
# This is a thin wrapper around the REAL house launcher, button.sh -
# unmodified, same file the real taskbar's own Network cell "Browser"
# row runs.
set -eu
HOUSE="$(cd "$(dirname "$0")" && pwd)"

# REAL FIX 2026-09-01, found live: the real house always already has
# this directory; a fresh standalone checkout doesn't. Without it,
# network_browser_manager.c's every real fopen("w") for its state/
# request/status/projection files fails silently (no error, no
# crash - it just never writes anything real), and the window renders
# nothing but its own title bar. This one mkdir -p is the only real
# difference between this demo and the actual house environment.
mkdir -p "$HOUSE/#.desktop"

if [ ! -x "$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x" ] || \
   [ ! -x "$HOUSE/&.hq-apps/network/+x/network_browser_manager.+x" ]; then
    echo "-- building --"
    sh "$HOUSE/build.sh"
fi

exec sh "$HOUSE/&.hq-apps/network/button.sh" "$HOUSE"
