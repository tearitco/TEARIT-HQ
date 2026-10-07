#!/bin/sh
# nb_write_submit.sh - submit a form for the network-browser manager.
# Invoked by a BUTTON row's <item> action: argv is
#   submit <action_url> <method> <package_dir> <house_root>
# ($1=submit $2=action $3=method $4=package_dir $5=house_root).
# GET: builds action?name=value&... from the LAST value per field name in
# #.desktop/network_browser_fields.txt (URL-encoded) and writes a go:
# request. Anything else (post/...): v1 supports GET only - reports to
# the console file and navigates nowhere rather than sending wrongly.
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
CONSOLE_FILE="$DESKTOP_DIR/network_browser_console.txt"
case "$METHOD" in
    ""|get|GET)
        PAIRS=""
        if [ -f "$DESKTOP_DIR/network_browser_fields.txt" ]; then
            if command -v python3 >/dev/null 2>&1; then
                PAIRS="$(python3 -c "
import sys, urllib.parse
v = {}
for line in open(sys.argv[1], errors='replace'):
    if '\t' in line:
        k, val = line.rstrip('\n').split('\t', 1)
        v[k] = val
print(urllib.parse.urlencode(v))
" "$DESKTOP_DIR/network_browser_fields.txt")"
            else
                PAIRS="$(awk -F'\t' 'NF==2 {v[$1]=$2} END {first=1; for (k in v) { if (!first) printf "&"; first=0; printf "%s=%s", k, v[k] } }' "$DESKTOP_DIR/network_browser_fields.txt")"
            fi
        fi
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
        printf 'submit: %s forms not yet supported (v1: GET only)\n' "$METHOD" >> "$CONSOLE_FILE"
        ;;
esac
