#!/bin/bash
# open_event_ez.sh — context-menu launch of event-ez for one monster package.
# Usage (from tp_desktop_window METHOD dispatch):
#   open_event_ez.sh <package_dir> [house_root]
# Sets EZ_PKG_NAME / EZ_PKG_DIR and starts event-ez widget (GL).
set -e
PKG_DIR="${1:-}"
HOUSE_ROOT_ARG="${2:-}"
if [ -z "$PKG_DIR" ] || [ ! -d "$PKG_DIR" ]; then
    echo "open_event_ez: need entity package dir as argv[1]" >&2
    exit 1
fi
PKG_DIR="$(cd "$PKG_DIR" && pwd)"
NAME="$(basename "$PKG_DIR")"
# REAL FIX 2026-08-10, direct report ("event-ez button no longer opens
# event editor - path issue, should use relatives not absolutes"): the
# OLD "climb 4 fixed .. from package_dir" logic assumed every entity
# lives at .../MUCHI_RANCHER/entities/<name> (dev-tree depth). Pals-
# migrated entities live at xyzfs/users/<uuid>/home/livedesk/pals/<name>
# - a completely different depth - so the climb landed somewhere inside
# xyzfs/, nowhere near the real house root, and this script's own
# executable-check below failed silently. tp_desktop_window.c's METHOD
# dispatch now passes house_root explicitly as argv[2] - use that
# directly when given. The old climb is kept ONLY as a fallback for any
# caller that hasn't been updated to pass argv[2] yet.
if [ -n "$HOUSE_ROOT_ARG" ] && [ -d "$HOUSE_ROOT_ARG" ]; then
    HOUSE_ROOT="$(cd "$HOUSE_ROOT_ARG" && pwd)"
else
    # entity package lives at .../MUCHI_RANCHER/entities/<name> (dev-tree
    # only - do not rely on this for pals-migrated entities)
    MUCHI_RANCHER="$(cd "$PKG_DIR/../.." && pwd)"
    HOUSE_ROOT="$(cd "$MUCHI_RANCHER/../.." && pwd)"
fi
EZ="$HOUSE_ROOT/&.widgits/event-ez"
EVENT_PKG="$PKG_DIR/event_pkg"
mkdir -p "$EVENT_PKG/pages/page_1"

if [ ! -x "$EZ/button.sh" ] && [ ! -f "$EZ/button.sh" ]; then
    echo "open_event_ez: missing $EZ/button.sh" >&2
    exit 1
fi
# ensure ops built
if [ ! -x "$EZ/ops/+x/ez_compose_frame.+x" ]; then
    (cd "$EZ" && sh button.sh compile) || true
fi

export EZ_PKG_NAME="$NAME"
export EZ_PKG_DIR="$EVENT_PKG"
# setsid so taskbar/entity timeout does not kill the editor
setsid nohup env EZ_PKG_NAME="$NAME" EZ_PKG_DIR="$EVENT_PKG" \
    sh "$EZ/button.sh" r >/tmp/event-ez-"$NAME".log 2>&1 < /dev/null &
disown
echo "event-ez launched for $NAME (EZ_PKG_DIR=$EVENT_PKG) log=/tmp/event-ez-$NAME.log"
