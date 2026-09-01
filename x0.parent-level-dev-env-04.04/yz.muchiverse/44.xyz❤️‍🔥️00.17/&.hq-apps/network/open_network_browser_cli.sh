#!/bin/bash
# open_network_browser_cli.sh <house_root> [start_url]
#
# Real CLI/headless mirror of the network browser (CENTROID_GOLD_STD.md
# proof, 2026-08-31) - starts the same real manager if it isn't already
# running, then runs the ASCII renderer in the current terminal. Same
# real manager, same real state/action files, as
# open_network_browser.sh's own X11 window - proves the "two symmetric
# renderers over one centroid" claim live, not just by inspection.
set -u
HOUSE="$(readlink -f "$1")"
START_URL="${2:-}"
MGR="$HOUSE/&.hq-apps/network/+x/network_browser_manager.+x"
ASCII="$HOUSE/&.hq-apps/network/+x/network_browser_render_ascii.+x"

if [ ! -x "$MGR" ] || [ ! -x "$ASCII" ]; then
  bash "$HOUSE/&.hq-apps/network/build.sh" || exit 1
fi

if ! pgrep -f 'hq-apps/network/\+x/network_browser_manager\.\+x' >/dev/null 2>&1; then
  setsid nohup "$MGR" "$HOUSE" >/dev/null 2>&1 &
  echo "$!" >> "$HOUSE/#.desktop/livedesk_launched_pids.txt" 2>/dev/null
  sleep 0.3
fi

exec "$ASCII" "$HOUSE" $START_URL
