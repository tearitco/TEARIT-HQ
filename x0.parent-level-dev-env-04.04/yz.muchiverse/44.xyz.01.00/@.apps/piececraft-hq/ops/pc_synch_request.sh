#!/bin/sh
# pc_synch_request.sh taskbar|pchq
#
# One page file: the active livedesk desk .pdl (session.pdl names the
# book and the page). Both directions write that same file.
#   pchq    — board draws that desk's entities, and its book:/page:
#             labels switch to the same names the taskbar reads.
#   taskbar — hero_01 is written into that desk file at the hero's
#             current cell (replaced if the row is already there).
set -u
SELF=$(cd "$(dirname "$0")" && pwd)
HOUSE=$(cd "$SELF/../../.." && pwd)
FROM=${1:-pchq}
PCHQ="$HOUSE/@.apps/piececraft-hq"
OUT="$PCHQ/pieces/display/synched_entities.txt"
OPEN="$PCHQ/pieces/display/open_book_page.txt"
mkdir -p "$PCHQ/pieces/display" "$(dirname "$HOUSE/#.desktop/pc_synch_request.txt")"

SROOT=$(ls -d "$HOUSE"/xyzfs/users/*/home/livedesk/sessions 2>/dev/null | head -1)
[ -n "$SROOT" ] || exit 1
ACTIVE=$(sed -n 's/.*active_session | //p' "$SROOT/session.pdl" | head -1 | tr -d ' \r')
SESS="$SROOT/$ACTIVE/session.pdl"
DESK=$(sed -n 's/.*active_desk | //p' "$SESS" | head -1 | tr -d ' \r')
BOOK=$(sed -n 's/.*| name | //p' "$SESS" | head -1 | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
[ -n "$BOOK" ] || BOOK=$ACTIVE
PDL="$SROOT/$ACTIVE/desks/$DESK.pdl"

write_entities() {
    : > "$OUT"
    [ -f "$PDL" ] || return 0
    awk -F'|' '
        $1 ~ /DESK/ {
            name=$2; x=$4; y=$5
            gsub(/^[ \t]+|[ \t]+$/, "", name)
            gsub(/^[ \t]+|[ \t]+$/, "", x)
            gsub(/^[ \t]+|[ \t]+$/, "", y)
            if (name == "" || seen[name]++) next
            xi=x+0; yi=y+0
            if (xi < 0) xi = -xi
            if (yi < 0) yi = -yi
            if (xi >= 40) xi = int(xi / 80)
            if (yi >= 40) yi = int(yi / 80)
            print name, xi, yi
        }
    ' "$PDL" > "$OUT"
}

if [ "$FROM" = pchq ]; then
    write_entities
    printf 'book=%s\npage=%s\npdl=%s\n' "$BOOK" "$DESK" "$PDL" > "$OPEN"
else
    H="$PCHQ/pieces/hero_01/state.txt"
    hx=$(sed -n 's/^pos_x=//p' "$H" | head -1)
    hy=$(sed -n 's/^pos_y=//p' "$H" | head -1)
    hx=${hx:-0}; hy=${hy:-0}
    px=$((hx * 80)); py=$((hy * 80))
    if [ -f "$PDL" ]; then
        grep -v '| hero_01 |' "$PDL" > "$PDL.tmp" || true
        printf 'DESK | hero_01 | @.apps/piececraft-hq/pieces/hero_01 | %s | %s | 0 | 0 | H | 0\n' "$px" "$py" >> "$PDL.tmp"
        mv "$PDL.tmp" "$PDL"
    fi
    write_entities
    printf 'book=%s\npage=%s\npdl=%s\n' "$BOOK" "$DESK" "$PDL" > "$OPEN"
fi
LAST="$HOUSE/#.desktop/last_pchq_book_page.txt"
LB=$(sed -n 's/^book=//p' "$LAST" 2>/dev/null | head -1)
LP=$(sed -n 's/^page=//p' "$LAST" 2>/dev/null | head -1)
printf 'from=%s\nbook=%s\npage=%s\ndesk=%s\npdl=%s\nlast_book=%s\nlast_page=%s\ntime=%s\nstatus=entities\n' \
  "$FROM" "$BOOK" "$DESK" "$DESK" "$PDL" "$LB" "$LP" "$(date +%H:%M:%S)" \
  > "$HOUSE/#.desktop/pc_synch_request.txt"
