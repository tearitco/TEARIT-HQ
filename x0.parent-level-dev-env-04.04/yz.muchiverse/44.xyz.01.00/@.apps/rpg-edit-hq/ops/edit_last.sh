#!/bin/sh
# Last book the editor opened. One row, then the demo.
# save    <statefile> <book> <page> --house <house> [--maps <dir>]
# resolve <statefile> --house <house> --demo-book <id> --demo-page <id> [--maps <dir>]
# A page is real when @.apps/<book>/rooms.pdl has "ROOM | <page>"
# or <maps>/<book>/<page>/map.txt exists.
# resolve prints one line: book=<id> page=<id>
# A missing row, a missing book, or a missing page prints the demo and exits 0.
# save writes nothing and exits 2 when the pair is not real.
set -u
ok_id() {
    printf '%s' "$1" | awk 'BEGIN{ok=0} /^[A-Za-z0-9_-]{1,48}$/{ok=1} END{exit !ok}'
}
page_real() {
    house=$1; maps=$2; book=$3; page=$4
    rooms="$house/@.apps/$book/rooms.pdl"
    if [ -f "$rooms" ]; then
        awk -F'|' -v id="$page" '
            $1 ~ /^ROOM[[:space:]]*$/ {
                gsub(/^[ \t]+|[ \t]+$/, "", $2)
                if ($2 == id) { found=1; exit }
            }
            END { exit !found }
        ' "$rooms" && return 0
    fi
    if [ -n "$maps" ] && [ -f "$maps/$book/$page/map.txt" ]; then
        return 0
    fi
    return 1
}
need() {
    echo "edit_last: $1" >&2
    exit 2
}
cmd=${1:-}
shift || need "usage: save|resolve"
state=${1:-}; shift || need "need a state file"
house=""; maps=""; demo_book=""; demo_page=""
book=""; page=""
while [ $# -gt 0 ]; do
    case "$1" in
        --house) house=${2:-}; shift 2 ;;
        --maps) maps=${2:-}; shift 2 ;;
        --demo-book) demo_book=${2:-}; shift 2 ;;
        --demo-page) demo_page=${2:-}; shift 2 ;;
        *)
            if [ "$cmd" = save ] && [ -z "$book" ]; then book=$1; shift
            elif [ "$cmd" = save ] && [ -z "$page" ]; then page=$1; shift
            else need "unknown argument"; fi
            ;;
    esac
done
[ -n "$house" ] || need "need --house"
case "$cmd" in
save)
    ok_id "$book" && ok_id "$page" || need "bad book or page id"
    page_real "$house" "$maps" "$book" "$page" || need "no such book page"
    dir=$(dirname "$state")
    mkdir -p "$dir" || need "cannot make the state directory"
    tmp="$state.tmp.$$"
    printf 'book=%s\npage=%s\n' "$book" "$page" > "$tmp" || need "cannot write"
    mv "$tmp" "$state" || need "cannot save"
    printf 'book=%s page=%s\n' "$book" "$page"
    ;;
resolve)
    ok_id "$demo_book" && ok_id "$demo_page" || need "bad demo id"
    page_real "$house" "$maps" "$demo_book" "$demo_page" || need "demo book page is not real"
    book=""; page=""
    if [ -f "$state" ]; then
        book=$(awk -F= '/^book=/{print substr($0,6); exit}' "$state")
        page=$(awk -F= '/^page=/{print substr($0,6); exit}' "$state")
    fi
    if ! ok_id "$book" || ! ok_id "$page" || ! page_real "$house" "$maps" "$book" "$page"; then
        book=$demo_book
        page=$demo_page
    fi
    printf 'book=%s page=%s\n' "$book" "$page"
    ;;
*)
    need "usage: save|resolve"
    ;;
esac
