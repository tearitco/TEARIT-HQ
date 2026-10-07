#!/bin/sh
# nb_write_toggle.sh - flip a checkbox/radio for the network-browser
# manager. Invoked by a check row's <item> action: argv is
#   toggle <name> <submitvalue> <default-on> <package_dir> <house_root>
# ($1..$4 fixed, $5=package_dir, $6=house_root).
# Appends "name<TAB>on|off" to #.desktop/network_browser_checks.txt using
# the last recorded state (else the page default). The submit script
# includes name=submitvalue only when on; the projector re-reads this
# file so the [x]/[ ] updates live. Cleared per fetch like fields.
set -u
if [ $# -ne 6 ] || [ "$1" != "toggle" ]; then
    echo "nb_write_toggle.sh: usage: toggle <name> <submitvalue> <default-on> <package_dir> <house_root>" >&2
    exit 1
fi
NAME="$2"
HOUSE_ROOT="$6"
DEFAULT="$4"
if [ -z "$HOUSE_ROOT" ] || [ -z "$NAME" ]; then
    echo "nb_write_toggle.sh: missing house_root or name" >&2
    exit 1
fi
CHECKS="$HOUSE_ROOT/#.desktop/network_browser_checks.txt"
LAST=""
if [ -f "$CHECKS" ]; then
    LAST="$(awk -F'\t' -v n="$NAME" '$1==n {s=$2} END {print s}' "$CHECKS")"
fi
if [ -z "$LAST" ]; then
    if [ "$DEFAULT" = "1" ]; then CUR="on"; else CUR="off"; fi
else
    CUR="$LAST"
fi
if [ "$CUR" = "on" ]; then NEW="off"; else NEW="on"; fi
TAB="$(printf '\t')"
printf '%s\t%s\n' "$NAME" "$NEW" >> "$CHECKS"
