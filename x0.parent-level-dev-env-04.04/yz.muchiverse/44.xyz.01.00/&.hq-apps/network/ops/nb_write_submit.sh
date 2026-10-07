#!/bin/sh
# nb_write_submit.sh - submit a form for the network-browser manager.
# Invoked by a BUTTON row's <item> action: argv is
#   submit <action_url> <method> <package_dir> <house_root>
# ($1=submit $2=action $3=method $4=package_dir $5=house_root).
# Field values: hidden defaults come from the live page.state HIDDEN rows
# (stable across ticks); interactively committed values from
# network_browser_fields.txt win on name collision. Both feed one
# url-encoded pair set shared by GET (go: with query) and POST (post:
# with body). The manager clears the fields file on every fetch.
set -u
if [ $# -ne 5 ] || [ "$1" != "submit" ]; then
    echo "nb_write_submit.sh: usage: submit <action_url> <method> <package_dir> <house_root>" >&2
    exit 1
fi
ACTION="$2"
METHOD="$3"
HOUSE_ROOT="$5"
if [ -z "$HOUSE_ROOT" ] || [ -z "$ACTION" ]; then
    echo "nb_write_submit.sh: missing house_root or action" >&2
    exit 1
fi
DESKTOP_DIR="$HOUSE_ROOT/#.desktop"
REQUEST_FILE="$DESKTOP_DIR/network_browser_request.txt"

MERGED="$DESKTOP_DIR/network_browser_fields.txt"
CLEANUP_MERGED=""
if grep -q "^HIDDEN|" "$DESKTOP_DIR/network_browser_page.state.txt" 2>/dev/null; then
    HIDDEN_TMP="$(mktemp)"
    grep "^HIDDEN|" "$DESKTOP_DIR/network_browser_page.state.txt" | sed 's/^HIDDEN|//;s/|/\t/' > "$HIDDEN_TMP"
    MERGED="$(mktemp)"
    cat "$HIDDEN_TMP" "$DESKTOP_DIR/network_browser_fields.txt" 2>/dev/null > "$MERGED"
    rm -f "$HIDDEN_TMP"
    CLEANUP_MERGED=1
fi
PAIRS=""
if [ -f "$MERGED" ]; then
    if command -v python3 >/dev/null 2>&1; then
        PAIRS="$(python3 -c "
import sys, urllib.parse
v = {}
for line in open(sys.argv[1], errors='replace'):
    if '\t' in line:
        k, val = line.rstrip('\n').split('\t', 1)
        v[k] = val
print(urllib.parse.urlencode(v))
" "$MERGED")"
    else
        PAIRS="$(awk -F'\t' 'NF==2 {v[$1]=$2} END {first=1; for (k in v) { if (!first) printf "&"; first=0; printf "%s=%s", k, v[k] } }' "$MERGED")"
    fi
fi
[ -n "$CLEANUP_MERGED" ] && rm -f "$MERGED"

case "$METHOD" in
    ""|get|GET)
        case "$ACTION" in
            *\?*) SEP="&" ;;
            *) SEP="?" ;;
        esac
        if [ -n "$PAIRS" ]; then
            printf "go:%s%s%s\n" "$ACTION" "$SEP" "$PAIRS" > "$REQUEST_FILE"
        else
            printf "go:%s\n" "$ACTION" > "$REQUEST_FILE"
        fi
        ;;
    *)
        TAB="$(printf '\t')"
        printf 'post:%s\t%s\n' "$ACTION" "$PAIRS" > "$REQUEST_FILE"
        ;;
esac
