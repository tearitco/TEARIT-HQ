#!/bin/sh
# nb_write_file.sh - pick a file for <input type=file> and commit its path.
# Invoked by a FILE row's <item> action: argv is
#   file <name> <package_dir> <house_root>
# ($1=file, $2=name, $3=package_dir, $4=house_root).
#
# Opens the house file-explorer through its documented modal contract
# (&.widgits/file-explorer/fe-pick.sh LOAD <dir>) and appends
# name<TAB>/abs/path to #.desktop/network_browser_fields.txt - the same
# file text inputs write to. Submit reads it back and turns the form
# into a multipart POST when any committed value is an existing file.
#
# Cancelling the picker writes nothing: the row keeps whatever it had.
# The manager clears the fields file on every fetch, so a stale path can
# never leak into the next page.
set -u
if [ $# -ne 4 ] || [ "$1" != "file" ]; then
    echo "nb_write_file.sh: usage: file <name> <package_dir> <house_root>" >&2
    exit 1
fi
NAME="$2"
HOUSE_ROOT="$4"
if [ -z "$HOUSE_ROOT" ] || [ -z "$NAME" ]; then
    echo "nb_write_file.sh: missing house_root or name" >&2
    exit 1
fi

PICKER="$HOUSE_ROOT/&.widgits/file-explorer/fe-pick.sh"
if [ ! -f "$PICKER" ]; then
    echo "file: picker not found at $PICKER" >> "$HOUSE_ROOT/#.desktop/network_browser_console.txt"
    exit 0
fi

# Start in the user's own space when it exists - uploads are almost always
# of something the user made, not of a system path.
START="$HOUSE_ROOT/#.desktop"
[ -d "$START" ] || START="$HOUSE_ROOT"

PICK="$(sh "$PICKER" LOAD "$START" 180 2>/dev/null | head -n1)"
if [ -z "$PICK" ]; then
    echo "file: pick cancelled" >> "$HOUSE_ROOT/#.desktop/network_browser_console.txt"
    exit 0
fi
if [ ! -f "$PICK" ]; then
    printf 'file: %s is not a readable file\n' "$PICK" >> "$HOUSE_ROOT/#.desktop/network_browser_console.txt"
    exit 0
fi

FIELDS="$HOUSE_ROOT/#.desktop/network_browser_fields.txt"
TAB="$(printf '\t')"
printf '%s\t%s\n' "$NAME" "$PICK" >> "$FIELDS"