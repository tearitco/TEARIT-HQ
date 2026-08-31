#!/bin/bash
# open_network_app.sh <house_root> <app> <title>
#
# Opens one of the house networking apps (pal-chat-irc / pal-forum /
# pal-chain) in a fresh gnome-terminal tab, same transport as the
# taskbar's own open_cli.sh here:
#   *.monads/*.livedesk-taskbar/ops/open_cli.sh
# Each app is a session-isolated chtpm app that renders ASCII frames in
# a terminal under system/orchestrator (launched by `button.sh run`), so
# a terminal tab IS its current UI until the Phase-2 khtpm-window
# conversion (NETWORK-CELL-HQ-WINDOWS-DESIGN.md).
#
# App dirs are LITERAL (emoji names, no '*' characters) so resolving
# them from house_root and feeding them to gnome-terminal is exec-safe
# (the open_cli.sh star-path bug does not apply). Never depends on cwd.
set -u
HOUSE="$(readlink -f "$1")"
APP="$2"
TITLE="${3:-network}"

case "$APP" in
  irc)   APPDIR="$HOUSE/044.pal-chat-irc👥️+2" ;;
  forum) APPDIR="$HOUSE/041.pal-forum👥️" ;;
  chain) APPDIR="$HOUSE/041.pal-chain⛓️" ;;
  *) echo "open_network_app: unknown app key '$APP'" >&2; exit 1 ;;
esac

if [ ! -x "$APPDIR/button.sh" ]; then
  echo "open_network_app: button.sh missing at $APPDIR" >&2
  exit 1
fi

exec gnome-terminal --title="$TITLE" -- bash -c "bash '$APPDIR/button.sh' run"