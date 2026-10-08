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
# 2. checked boxes. The checks file is OPTIONAL - a page whose boxes are
# all page-defaults never produces one, and gating on it silently dropped
# every pre-checked box. A missing file reads as "no toggle yet".
: > /dev/null
CHECKS_FILE="$DESKTOP_DIR/network_browser_checks.txt"
[ -f "$CHECKS_FILE" ] || CHECKS_FILE=/dev/null
if [ -f "$DESKTOP_DIR/network_browser_page.state.txt" ]; then
    while IFS= read -r line; do
        kind="${line%%|*}"
        [ "$kind" = "INPUT" ] || continue
        rest="${line#*|}"
        nm="${rest%%|*}"
        [ -n "$nm" ] || continue
        typ="${rest#*|}"; typ="${typ%%|*}"
        case "$typ" in checkbox|radio) ;; *) continue ;; esac
        st="$(awk -F'\t' -v n="$nm" '$1==n {s=$2} END {print s}' "$CHECKS_FILE" 2>/dev/null)"
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
# 2b. untouched text-ish inputs submit their page default (INPUT row field 3)
# the way a real browser does. Typed values land in step 3 and win ties.
# Pipes are 0x7f on the wire (see extractor) so a value can never split a
# field; translate back here, at the last moment before urlencoding.
awk -F'|' '$1=="INPUT" && ($3=="text"||$3=="search"||$3=="email"||$3=="url"||$3=="number"||$3=="tel"||$3=="password"||$3=="textarea") {
    v=$4; gsub("\177", "|", v);
    if (v != "") printf "%s\t%s\n", $2, v
}' "$DESKTOP_DIR/network_browser_page.state.txt" >> "$MERGED_ACCUM"
# 3. typed fields last (most interactive source wins ties)
cat "$DESKTOP_DIR/network_browser_fields.txt" 2>/dev/null >> "$MERGED_ACCUM"
MERGED="$MERGED_ACCUM"
CLEANUP_MERGED=1
# Validation gate (2026-10-07, M4 follow-on): never send a knowingly-invalid
# form. Two rules, both driven by the live page.state INPUT rows:
#   1. required (field 5 == 1) text-ish fields must have a non-empty value
#   2. email/url/number values must match a loose HTML-spec shape even when
#      the field is optional - an empty optional field passes silently
# BLOCKED accumulates " name" for rule 1, " name(reason)" for rule 2.
BLOCKED=""
while IFS= read -r line; do
    case "$line" in INPUT\|*) ;; *) continue ;; esac
    nm="${line#INPUT|}";  nm="${nm%%|*}"
    typ="${line#INPUT|*}"; typ="${typ#*|}";  typ="${typ%%|*}"
    # required is the LAST field on text-ish INPUT rows ("...|name|type|val|ph|req").
    # Toggle rows carry only 4 fields, so the tail is never "1" for them.
    req="${line##*|}"; [ "$req" = "1" ] || req=0
    case "$typ" in text|search|textarea|email|url|number) ;; *) continue ;; esac
    val="$(awk -F'\t' -v n="$nm" '$1==n {v=$2} END {print v}' "$DESKTOP_DIR/network_browser_fields.txt" 2>/dev/null)"
    if [ -z "$val" ]; then
        [ "$req" = "1" ] && BLOCKED="$BLOCKED $nm"
        continue
    fi
    case "$typ" in
        email) case "$val" in *@*.*) ;; *) BLOCKED="$BLOCKED $nm(email)" ;; esac ;;
        url)   case "$val" in *://*) ;; *) BLOCKED="$BLOCKED $nm(url)" ;; esac ;;
        number)
            case "$val" in -*) v="${val#-}" ;; *) v="$val" ;; esac
            case "$v" in ""|*[!0-9.]*) BLOCKED="$BLOCKED $nm(number)" ;; *) ;; esac ;;
    esac
done < "$DESKTOP_DIR/network_browser_page.state.txt"
if [ -n "$BLOCKED" ]; then
    printf 'submit: blocked, invalid form:%s\n' "$BLOCKED" >> "$DESKTOP_DIR/network_browser_console.txt"
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
