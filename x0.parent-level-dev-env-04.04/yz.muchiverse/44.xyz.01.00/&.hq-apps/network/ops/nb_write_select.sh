#!/bin/sh
# nb_write_select.sh - choose a <select> option for the network-browser
# manager. Invoked by a SELECT option row's <item> action: argv is
#   select <name> <value> <package_dir> <house_root>
# ($1=select, $2=name, $3=value, $4=package_dir, $5=house_root).
#
# Appends "name<TAB>value" to #.desktop/network_browser_fields.txt - the
# same file the text inputs write to - so nb_write_submit.sh picks the
# choice up with no new concept, and a later pick for the same select
# replaces the earlier one (submit dedupes by name, last wins).
#
# The manager clears the fields file on every fetch, so a stale choice can
# never leak into the next page.
set -u
if [ $# -ne 5 ] || [ "$1" != "select" ]; then
    echo "nb_write_select.sh: usage: select <name> <value> <package_dir> <house_root>" >&2
    exit 1
fi
NAME="$2"
VALUE="$3"
HOUSE_ROOT="$5"
if [ -z "$HOUSE_ROOT" ] || [ -z "$NAME" ]; then
    echo "nb_write_select.sh: missing house_root or name" >&2
    exit 1
fi
FIELDS="$HOUSE_ROOT/#.desktop/network_browser_fields.txt"
TAB="$(printf '\t')"
printf '%s\t%s\n' "$NAME" "$VALUE" >> "$FIELDS"