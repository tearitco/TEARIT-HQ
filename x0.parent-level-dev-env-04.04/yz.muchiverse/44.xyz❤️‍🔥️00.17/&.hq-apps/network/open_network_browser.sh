#!/bin/bash
# open_network_browser.sh <house_root>
#
# Network cell "Browser" row launcher - a cli-io window STUB
# (NETWORK-CELL-HQ-WINDOWS-DESIGN.md §5.3). Opens the standalone
# cli_io_window X11 console container; it hosts a real shell today but
# has NO web functionality yet (deliberately out of scope).
set -u
HOUSE="$(readlink -f "$1")"
BIN="$HOUSE/&.hq-apps/network/+x/cli_io_window.+x"

if [ ! -x "$BIN" ]; then
  bash "$HOUSE/&.hq-apps/network/build.sh" || exit 1
fi

# Single-instance guard (same shape as the HQ app launchers).
if pgrep -f 'hq-apps/network/\+x/cli_io_window\.\+x' >/dev/null 2>&1; then
  exit 0
fi

setsid nohup "$BIN" "browser (cli-io stub)" >/dev/null 2>&1 &
echo "$!" >> "$HOUSE/#.desktop/livedesk_launched_pids.txt" 2>/dev/null