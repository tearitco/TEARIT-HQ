#!/bin/bash
# run.sh - launch the demo standalone. house_root = this directory.
set -eu
HOUSE="$(cd "$(dirname "$0")" && pwd)"
NET="$HOUSE/&.hq-apps/network"
CHTPM="$NET/network-browser-hq.chtpm"

mkdir -p "$HOUSE/#.desktop"

if [ ! -x "$HOUSE/khtpm_generic_host.+x" ] || [ ! -x "$NET/+x/network_browser_manager.+x" ]; then
    echo "-- building --"
    sh "$HOUSE/build.sh"
fi

# Real, minimal self-heal (same real reasoning as the house's own
# button.sh): the manager overwrites $CHTPM continuously with its own
# live projection, so the checked-in bootstrap only needs to seed it
# once, or restore it if it's ever missing.
if [ ! -f "$CHTPM" ]; then
    cp "$NET/network-browser-hq.chtpm.bootstrap" "$CHTPM"
fi

exec "$HOUSE/khtpm_generic_host.+x" "$HOUSE" "$CHTPM"
