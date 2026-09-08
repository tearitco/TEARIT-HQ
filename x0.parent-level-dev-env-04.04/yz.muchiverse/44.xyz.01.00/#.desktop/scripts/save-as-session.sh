#!/bin/sh
# save-as-session.sh <house_root> - the strip file cell's "save-as" action.
#
# Opens the File Explorer widget (modal, SAVE mode) rooted at the livedesk
# sessions dir, waits for the user to type a name, and drops that bare
# name in #.desktop/livedesk_pending_save_as.txt for the taskbar
# manager's ktb_poll_pending_save_as() to consume + run
# livedesk_save_as_with_name().
#
# Symmetric with pick-session.sh (the "load" action).
set -u
HOUSE="${1:-}"
[ -n "$HOUSE" ] && [ -d "$HOUSE" ] || { HOUSE="$(cd "$(dirname "$0")/../.." && pwd)"; }

FE="$HOUSE/&.widgits/file-explorer"

SROOT=""
for d in "$HOUSE"/xyzfs/users/*/home/livedesk/sessions; do
    [ -d "$d" ] && { SROOT="$d"; break; }
done
[ -n "$SROOT" ] || SROOT="$(find "$HOUSE/xyzfs" -type d -name sessions 2>/dev/null | head -n1)"
[ -n "$SROOT" ] && [ -d "$SROOT" ] || exit 0

PICK="$(sh "$FE/fe-pick.sh" SAVE "$SROOT")"
[ -n "$PICK" ] || exit 0

# SAVE mode returns <start_dir>/<typed-name> (or just the name); we only
# want the leaf, and only a plain name (no separators).
NAME="$(basename "$PICK")"
case "$NAME" in
    ''|*/*|*\\*|..) exit 0 ;;
esac

printf '%s\n' "$NAME" > "$HOUSE/#.desktop/livedesk_pending_save_as.txt"
