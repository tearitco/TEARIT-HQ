#!/bin/sh
# boot.sh - autostart entry for the desk hotbar ($.crypts/autostart.pdl LAUNCH row).
# Waits (up to 30 s) for the dock to publish its geometry (#.desktop/dock_stack/base.txt), so the
# hotbar opens already docked above the bottom bar, then opens it.
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"
# already up (a second start of the desktop, a relaunch after the first one succeeded): nothing to do, and no 30 s wait. open_hotbar.sh --if-not-running
# re-checks under its lock, so two boots that race still open exactly one hotbar.
if [ -n "$(ps -eo pid,args | awk -v h="$HOUSE" 'NR > 1 { a0 = $2; if (a0 ~ /khtpm_core_render\.\+x$/ && index($0, h) && index($0, "hotbar-desk.xhtpm")) { print $1; exit } }')" ]; then exit 0; fi
i=0
while [ ! -s "$HOUSE/#.desktop/dock_stack/base.txt" ] && [ "$i" -lt 30 ]; do sleep 1; i=$((i+1)); done
exec sh "$HERE/open_hotbar.sh" "$HOUSE" desk --if-not-running
