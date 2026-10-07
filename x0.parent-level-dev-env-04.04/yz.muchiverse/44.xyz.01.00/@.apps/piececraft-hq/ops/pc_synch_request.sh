#!/bin/sh
# pc_synch_request.sh taskbar|pchq
#
# Book is the livedesk session. Page is that session's desk file.
# The sender keeps its book and page. The inheritor changes to match.
#   taskbar — every pc-hq takes the desk's book and page.
#   pchq    — the desk takes that board's book and page.
#             Other boards are left alone.
# Neither direction writes a hero_01 row.
set -u
SELF=$(cd "$(dirname "$0")" && pwd)
HOUSE=$(cd "$SELF/../../.." && pwd)
FROM=${1:-pchq}
PCHQ="$HOUSE/@.apps/piececraft-hq"
OPEN="$PCHQ/pieces/display/open_book_page.txt"
# The saved pdl= in open_book_page.txt is stored HOUSE-RELATIVE and resolved against the live house root on read
# (same rule as _shared-lib/khtpm_locations.c, so a moved/renamed/copied checkout still finds its page file).
rel_of() { case "$1" in "$HOUSE"/*) printf '%s' "${1#"$HOUSE"/}" ;; *) printf '%s' "$1" ;; esac; }
abs_of() {
    case "$1" in
        "") printf '' ;;
        /*) if [ -r "$1" ]; then printf '%s' "$1"
            else case "$1" in */xyzfs/*) printf '%s' "$HOUSE/xyzfs/${1#*/xyzfs/}" ;; *) printf '%s' "$1" ;; esac; fi ;;
        *) printf '%s' "$HOUSE/$1" ;;
    esac
}
mkdir -p "$PCHQ/pieces/display" "$(dirname "$HOUSE/#.desktop/pc_synch_request.txt")"

SROOT=""
ACTIVE=""
for d in "$HOUSE"/xyzfs/users/*/home/livedesk/sessions; do
    [ -f "$d/session.pdl" ] || continue
    a=$(sed -n 's/.*active_session | //p' "$d/session.pdl" | head -1 | tr -d ' \r')
    [ -n "$a" ] || continue
    [ -f "$d/$a/session.pdl" ] || continue
    SROOT=$d
    ACTIVE=$a
    break
done
[ -n "$SROOT" ] || exit 1
SESS="$SROOT/$ACTIVE/session.pdl"
DESK=$(sed -n 's/.*active_desk | //p' "$SESS" | head -1 | tr -d ' \r')
BOOK=$(sed -n 's/.*| name | //p' "$SESS" | head -1 | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
[ -n "$BOOK" ] || BOOK=$ACTIVE
PDL="$SROOT/$ACTIVE/desks/$DESK.pdl"

set_state_field() {
    file=$1 key=$2 val=$3
    tmp="$file.synch-tmp"
    if grep -q "| $key |" "$file" 2>/dev/null; then
        sed "s/| $key | .*/| $key | $val/" "$file" > "$tmp"
        mv "$tmp" "$file"
    else
        printf 'STATE | %s | %s\n' "$key" "$val" >> "$file"
    fi
}

if [ "$FROM" = taskbar ]; then
    # The desk is the sender. Every board of this app is an inheritor.
    printf 'source=desk\nbook=%s\npage=%s\npdl=%s\n' "$BOOK" "$DESK" "$(rel_of "$PDL")" > "$OPEN"
    STATUS=board-follows-desk
else
    # The board is the sender. Its page is open_book_page when that
    # file already names one, otherwise the loaded map (map_id) and
    # its desk (desk_id). The desk session is the inheritor.
    BB=$BOOK
    BP=$DESK
    BPDL=$PDL
    SRC=""
    if [ -f "$OPEN" ]; then
        SRC=$(sed -n 's/^source=//p' "$OPEN" | head -1 | tr -d ' \r')
    fi
    # source=board means the user picked a book or page inside pc-hq
    # after Synch. That pick is the sender. The old desk pin is not.
    if [ -f "$OPEN" ] && [ "$SRC" != board ]; then
        ob=$(sed -n 's/^book=//p' "$OPEN" | head -1)
        op=$(sed -n 's/^page=//p' "$OPEN" | head -1)
        od=$(sed -n 's/^pdl=//p' "$OPEN" | head -1)
        [ -n "$ob" ] && BB=$ob
        [ -n "$op" ] && BP=$op
        [ -n "$od" ] && BPDL=$(abs_of "$od")
    fi
    WS="$PCHQ/pieces/world_01/state.txt"
    if { [ ! -f "$OPEN" ] || [ "$SRC" = board ]; } && [ -f "$WS" ]; then
        mid=$(sed -n 's/^map_id=//p' "$WS" | head -1 | tr -d ' \r')
        did=$(sed -n 's/^desk_id=//p' "$WS" | head -1 | tr -d ' \r')
        [ -n "$mid" ] && BB=$mid
        [ -n "$did" ] && BP=$did
        BPDL=""
    fi
    # Same book: only the page changes. A different book is taken
    # only when that session directory is already on disk.
    if [ "$BB" != "$BOOK" ] && [ -d "$SROOT/$BB" ]; then
        set_state_field "$SROOT/session.pdl" active_session "$BB"
        set_state_field "$SROOT/$BB/session.pdl" name "$BB"
        SESS="$SROOT/$BB/session.pdl"
        ACTIVE=$BB
        BOOK=$BB
    fi
    DEST="$SROOT/$ACTIVE/desks/$BP.pdl"
    if [ -f "$DEST" ]; then
        set_state_field "$SESS" active_desk "$BP"
        STATUS=desk-follows-board
        PDL=$DEST
        DESK=$BP
        BOOK=$BB
    elif [ -n "$BPDL" ] && [ -f "$BPDL" ]; then
        # The board's page file lives under this book already.
        base=$(basename "$BPDL" .pdl)
        set_state_field "$SESS" active_desk "$base"
        STATUS=desk-follows-board
        PDL=$BPDL
        DESK=$base
    else
        STATUS=page-not-in-book
    fi
    if [ "$STATUS" = desk-follows-board ]; then
        printf 'source=desk\nbook=%s\npage=%s\npdl=%s\n' "$BOOK" "$DESK" "$(rel_of "$PDL")" > "$OPEN"
    fi
fi

LAST="$HOUSE/#.desktop/last_pchq_book_page.txt"
LB=$(sed -n 's/^book=//p' "$LAST" 2>/dev/null | head -1)
LP=$(sed -n 's/^page=//p' "$LAST" 2>/dev/null | head -1)
printf 'from=%s\nbook=%s\npage=%s\ndesk=%s\npdl=%s\nlast_book=%s\nlast_page=%s\ntime=%s\nstatus=%s\n' \
  "$FROM" "$BOOK" "$DESK" "$DESK" "$PDL" "$LB" "$LP" "$(date +%H:%M:%S)" "$STATUS" \
  > "$HOUSE/#.desktop/pc_synch_request.txt"
