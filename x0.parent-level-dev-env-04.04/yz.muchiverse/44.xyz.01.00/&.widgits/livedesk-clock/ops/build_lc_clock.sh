#!/bin/sh
# build_lc_clock.sh — build the livedesk-clock system (au11-hq/15.clock-design.md §5 P1).
#
# Two binaries:
#   +x/lc_clock.+x              headless daemon + control plane (no Xlib)
#   +x/lc_reminder_popup.+x     X11 RGB reminder window + CSS (house standard)
#
# Mirrors the khtpm strip build style (build_khtpm_strip.sh): cd to ops,
# mkdir +x, CC=gcc CFLAGS="-std=c11 -Wall -O2", Xft via pkg-config.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

# SHARED-SOURCE-COMPILE-IN-PLACE.md (2026-09-09): the popup is an X11
# RGB window styled by the shared CSS parser (house standard, NOT GL —
# see lc_reminder_popup.c's header). Compile the canonical parser in
# place via -I; no local copy in this dir.
SHARED="$(cd "$(dirname "$0")/../../_shared-lib" && pwd)"

echo "-- lc_clock (daemon + control plane, headless) -> +x/lc_clock.+x"
$CC $CFLAGS -o +x/lc_clock.+x lc_clock.c

echo "-- lc_reminder_popup (X11 RGB window + CSS) -> +x/lc_reminder_popup.+x"
$CC $CFLAGS $(pkg-config --cflags xft) -I "$SHARED" -o +x/lc_reminder_popup.+x \
  lc_reminder_popup.c "$SHARED/khtpm_css_parser.c" -lX11 $(pkg-config --libs xft)

echo "OK +x/lc_clock.+x and +x/lc_reminder_popup.+x"
