#!/bin/sh
# nb_write_field.sh - record a committed form-field value for the
# network-browser manager. Invoked by a content <cli_io> row's action on
# Enter: the renderer appends <package_dir> <house_root> <typed_value>,
# so argv is: field <name> <package_dir> <house_root> <typed>.
# Appends "name<TAB>value" to #.desktop/network_browser_fields.txt;
# the submit script reads the LAST value per name. The manager clears the
# file on every fetch (fresh page, fresh form state).
set -u
if [ $# -ne 5 ] || [ "$1" != "field" ]; then
    echo "nb_write_field.sh: usage: field <name> <package_dir> <house_root> <typed>" >&2
    exit 1
fi
NAME="$2"
HOUSE_ROOT="$4"
VALUE="$5"
if [ -z "$HOUSE_ROOT" ] || [ -z "$NAME" ]; then
    echo "nb_write_field.sh: missing house_root or name" >&2
    exit 1
fi
FIELDS="$HOUSE_ROOT/#.desktop/network_browser_fields.txt"
TAB="$(printf '\t')"
printf '%s\t%s\n' "$NAME" "$VALUE" >> "$FIELDS"
