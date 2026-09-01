#!/bin/bash
# open_network_browser.sh <house_root>
#
# Network cell "Browser" row launcher - REAL CENTROID_GOLD_STD.md proof
# app (2026-08-31), replacing the old cli_io_window stub row (that
# binary is still built for anything else pointing at it, but the
# taskbar's own Browser row now opens this). Real manager (fetch +
# simple HTML extraction) + real khtpm Elem-tree X11 renderer - two
# separate, single-instance-guarded processes, same real contract as
# every other house manager+renderer pair (khtpm_hq_manager.c/db-hq).
set -u
HOUSE="$(readlink -f "$1")"
MGR="$HOUSE/&.hq-apps/network/+x/network_browser_manager.+x"
REND="$HOUSE/&.hq-apps/network/+x/network_browser_render.+x"

if [ ! -x "$MGR" ] || [ ! -x "$REND" ]; then
  bash "$HOUSE/&.hq-apps/network/build.sh" || exit 1
fi

if ! pgrep -f 'hq-apps/network/\+x/network_browser_manager\.\+x' >/dev/null 2>&1; then
  setsid nohup "$MGR" "$HOUSE" >/dev/null 2>&1 &
  echo "$!" >> "$HOUSE/#.desktop/livedesk_launched_pids.txt" 2>/dev/null
fi

if pgrep -f 'hq-apps/network/\+x/network_browser_render\.\+x' >/dev/null 2>&1; then
  exit 0
fi

setsid nohup "$REND" "$HOUSE" >/dev/null 2>&1 &
echo "$!" >> "$HOUSE/#.desktop/livedesk_launched_pids.txt" 2>/dev/null
