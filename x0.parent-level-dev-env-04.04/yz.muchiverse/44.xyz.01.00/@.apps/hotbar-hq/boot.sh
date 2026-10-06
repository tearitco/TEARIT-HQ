#!/bin/sh
# boot.sh - autostart entry for the desk hotbar ($.crypts/autostart.pdl LAUNCH row).
# Waits (up to 30 s) for the dock to publish its geometry (#.desktop/dock_stack/base.txt), so the
# hotbar opens already docked above the bottom bar, then opens it.
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"
i=0
while [ ! -s "$HOUSE/#.desktop/dock_stack/base.txt" ] && [ "$i" -lt 30 ]; do sleep 1; i=$((i+1)); done
exec sh "$HERE/open_hotbar.sh" "$HOUSE" desk
