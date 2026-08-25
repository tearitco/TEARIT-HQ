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

# Sync the shared CSS parser from the single canonical source (same
# build-time-copy rule as build_khtpm_strip.sh — see
# &.widgits/_shared-lib/README.md). The popup is an X11 RGB window
# styled by this parser (house standard, NOT GL — see the header note
# in lc_reminder_popup.c).
SHARED="$(cd "$(dirname "$0")/../../_shared-lib" && pwd)"
cp "$SHARED/khtpm_css_parser.c" khtpm_css_parser.c
cp "$SHARED/khtpm_css_parser.h" khtpm_css_parser.h

echo "-- lc_clock (daemon + control plane, headless) -> +x/lc_clock.+x"
$CC $CFLAGS -o +x/lc_clock.+x lc_clock.c

echo "-- lc_reminder_popup (X11 RGB window + CSS) -> +x/lc_reminder_popup.+x"
$CC $CFLAGS $(pkg-config --cflags xft) -o +x/lc_reminder_popup.+x \
  lc_reminder_popup.c khtpm_css_parser.c -lX11 $(pkg-config --libs xft)

echo "OK +x/lc_clock.+x and +x/lc_reminder_popup.+x"
