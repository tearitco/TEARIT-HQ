#!/bin/sh
# pick-session.sh <house_root> - the strip file cell's "load" action.
#
# Opens the File Explorer widget (modal, LOAD mode) rooted at the
# livedesk sessions dir, waits for a pick, and drops the chosen session
# id in #.desktop/livedesk_pending_open_session.txt for the taskbar
# manager's ktb_poll_pending_session_open() to consume + load.
#
# A "session" is a directory under the sessions root containing a
# session.pdl. The user may click that dir OR a file inside it - we
# resolve to the dir name either way.
set -u
HOUSE="${1:-}"
[ -n "$HOUSE" ] && [ -d "$HOUSE" ] || { HOUSE="$(cd "$(dirname "$0")/../.." && pwd)"; }

FE="$HOUSE/&.widgits/file-explorer"

# resolve the sessions root the same way the manager does:
# <house>/xyzfs/users/<active-user>/home/livedesk/pals is the pals root;
# sessions live at <house>/xyzfs/users/<uid>/home/livedesk/sessions.
# Fall back to a broad search if the layout differs.
SROOT=""
for d in "$HOUSE"/xyzfs/users/*/home/livedesk/sessions; do
    [ -d "$d" ] && { SROOT="$d"; break; }
done
[ -n "$SROOT" ] || SROOT="$(find "$HOUSE/xyzfs" -type d -name sessions 2>/dev/null | head -n1)"
[ -n "$SROOT" ] && [ -d "$SROOT" ] || exit 0

PICK="$(sh "$FE/fe-pick.sh" LOAD "$SROOT")"
[ -n "$PICK" ] || exit 0

# resolve to a session dir name
case "$PICK" in
    "$SROOT"/*) rel="${PICK#$SROOT/}"; ID="${rel%%/*}" ;;
    *)          ID="$(basename "$PICK")" ;;
esac
[ -n "$ID" ] || exit 0
[ -f "$SROOT/$ID/session.pdl" ] || exit 0

printf '%s\n' "$ID" > "$HOUSE/#.desktop/livedesk_pending_open_session.txt"
