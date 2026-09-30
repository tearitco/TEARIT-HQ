#!/bin/sh
# pc_synch_request.sh taskbar|pchq
# pchq: read the active livedesk desk and write its entities where the
# board draws them. taskbar: append the board's hero to that desk file
# if it is not already there.
set -u
SELF=$(cd "$(dirname "$0")" && pwd)
HOUSE=$(cd "$SELF/../../.." && pwd)
FROM=${1:-pchq}
PCHQ="$HOUSE/@.apps/piececraft-hq"
OUT="$PCHQ/pieces/display/synched_entities.txt"
mkdir -p "$PCHQ/pieces/display" "$(dirname "$HOUSE/#.desktop/pc_synch_request.txt")"

SROOT=$(ls -d "$HOUSE"/xyzfs/users/*/home/livedesk/sessions 2>/dev/null | head -1)
[ -n "$SROOT" ] || exit 1
ACTIVE=$(sed -n 's/.*active_session | //p' "$SROOT/session.pdl" | head -1 | tr -d ' \r')
SESS="$SROOT/$ACTIVE/session.pdl"
DESK=$(sed -n 's/.*active_desk | //p' "$SESS" | head -1 | tr -d ' \r')
PDL="$SROOT/$ACTIVE/desks/$DESK.pdl"

if [ "$FROM" = pchq ]; then
    : > "$OUT"
    [ -f "$PDL" ] || exit 0
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
else
    H="$PCHQ/pieces/hero_01/state.txt"
    hx=$(sed -n 's/^pos_x=//p' "$H" | head -1)
    hy=$(sed -n 's/^pos_y=//p' "$H" | head -1)
    hx=${hx:-0}; hy=${hy:-0}
    px=$((hx * 80)); py=$((hy * 80))
    if [ -f "$PDL" ] && ! grep -q '| hero_01 |' "$PDL"; then
        printf 'DESK | hero_01 | @.apps/piececraft-hq/pieces/hero_01 | %s | %s | 0 | 0 | H | 0\n' "$px" "$py" >> "$PDL"
    fi
fi
printf 'from=%s\ndesk=%s\ntime=%s\nstatus=entities\n' "$FROM" "$DESK" "$(date +%H:%M:%S)" \
  > "$HOUSE/#.desktop/pc_synch_request.txt"
