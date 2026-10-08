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

MERGED_ACCUM="$(mktemp)"
# 1. hidden defaults (page scope)
if grep -q "^HIDDEN|" "$DESKTOP_DIR/network_browser_page.state.txt" 2>/dev/null; then
    grep "^HIDDEN|" "$DESKTOP_DIR/network_browser_page.state.txt" | sed 's/^HIDDEN|//;s/|/\t/' >> "$MERGED_ACCUM"
fi
# 2. checked boxes (submit values from live page.state INPUT rows)
if [ -f "$DESKTOP_DIR/network_browser_checks.txt" ]; then
    while IFS= read -r line; do
        kind="${line%%|*}"
        [ "$kind" = "INPUT" ] || continue
        rest="${line#*|}"
        nm="${rest%%|*}"
        [ -n "$nm" ] || continue
        st="$(awk -F'\t' -v n="$nm" '$1==n {s=$2} END {print s}' "$DESKTOP_DIR/network_browser_checks.txt" 2>/dev/null)"
        if [ -z "$st" ]; then
            # no toggle yet: page default (INPUT row field 3)
            df="$(printf '%s' "$rest" | awk -F'|' '{print $3}')"
            [ "$df" = "checked" ] && st="on" || st="off"
        fi
        [ "$st" = "on" ] || continue
        sv="$(printf '%s' "$rest" | awk -F'|' '{print $4}')"
        [ -n "$sv" ] || sv="on"
        TAB2="$(printf '\t')"
        printf '%s\t%s\n' "$nm" "$sv" >> "$MERGED_ACCUM"
    done < "$DESKTOP_DIR/network_browser_page.state.txt"
fi
# 3. typed fields last (most interactive source wins ties)
cat "$DESKTOP_DIR/network_browser_fields.txt" 2>/dev/null >> "$MERGED_ACCUM"
MERGED="$MERGED_ACCUM"
CLEANUP_MERGED=1
# Required-field gate: every INPUT text/search/textarea row flagged
# required in the live page.state must have a non-empty committed value,
# else refuse with a console note (never send a knowingly-invalid form).
REQ_MISSING=""
while IFS= read -r line; do
    kind="${line%%|*}"
    [ "$kind" = "INPUT" ] || continue
    rest="${line#*|}"
    nm="${rest%%|*}"
    typ="$(printf '%s' "$rest" | awk -F'|' '{print $2}')"
    req="$(printf '%s' "$rest" | awk -F'|' '{print $5}')"
    case "$typ" in text|search|textarea) ;; *) continue ;; esac
    [ "$req" = "1" ] || continue
    val=""
    if [ -f "$DESKTOP_DIR/network_browser_fields.txt" ]; then
        val="$(awk -F'\t' -v n="$nm" '$1==n {v=$2} END {print v}' "$DESKTOP_DIR/network_browser_fields.txt")"
    fi
    if [ -z "$val" ]; then
        REQ_MISSING="$REQ_MISSING $nm"
    fi
done < "$DESKTOP_DIR/network_browser_page.state.txt"
if [ -n "$REQ_MISSING" ]; then
    printf 'submit: required field(s) empty:%s\n' "$REQ_MISSING" >> "$DESKTOP_DIR/network_browser_console.txt"
    [ -n "$CLEANUP_MERGED" ] && rm -f "$MERGED"
    exit 0
fi
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
