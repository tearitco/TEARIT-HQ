#!/bin/sh
# Read-only open of book:page. No window. Prints one line:
# book=<id> page=<id> event=<name> pages=<n> events=<n>
# A page is a desk id in <book>-pchq/.../maps/<book>/game.pdl.
# An event is the first directory under @.apps/<book>/events, or none.
# Exit 2 when the book or the page is not there.
set -u
need() { echo "edit_open: $1" >&2; exit 2; }
house=${1:-}; book=${2:-}; page=${3:-}
[ -n "$house" ] && [ -n "$book" ] && [ -n "$page" ] || need "usage: <house> <book> <page>"
ok() { printf '%s' "$1" | awk 'BEGIN{ok=0} /^[A-Za-z0-9_-]{1,48}$/{ok=1} END{exit !ok}'; }
ok "$book" && ok "$page" || need "bad book or page id"
game="$house/@.apps/$book/$book-pchq/pieces/system/maps/$book/game.pdl"
[ -f "$game" ] || need "no game.pdl for $book"
np=$(awk -F'|' -v id="$page" '
    $1 ~ /^GAME[[:space:]]*$/ {
        k=$2; v=$3
        gsub(/^[ \t]+|[ \t]+$/, "", k)
        gsub(/^[ \t]+|[ \t]+$/, "", v)
        if (k ~ /^desk_[0-9]+_id$/) { n++; if (v==id) hit=1 }
    }
    END { print n+0; exit !hit }
' "$game") || need "page $page is not a desk of $book"
evdir="$house/@.apps/$book/events"
event=none
ne=0
if [ -d "$evdir" ]; then
    event=$(find "$evdir" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' | sort | head -1)
    ne=$(find "$evdir" -mindepth 1 -maxdepth 1 -type d | wc -l | awk '{print $1}')
    [ -n "$event" ] || event=none
fi
printf 'book=%s page=%s event=%s pages=%s events=%s\n' "$book" "$page" "$event" "$np" "$ne"
