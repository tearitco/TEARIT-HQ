#!/bin/sh
# nb_write_clearautofill.sh - delete the network browser's autofill jar.
# Same argv convention as nb_write_clearhist.sh (1 explicit arg, argc=3,
# house_root is $3): the renderer appends <dir> <house_root> to the action.
#
# Autofill stores what you TYPED into forms, per origin. Clearing it is a
# real user need, not a nicety: the jar is a plain file in the house and
# this is the in-window way to drop it without leaving the browser.
set -e
if [ $# -lt 3 ]; then
    echo "nb_write_clearautofill.sh: unexpected argc ($#), expected 3" >&2
    exit 1
fi
HOUSE_ROOT="$3"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "nb_write_clearautofill.sh: bad house_root" >&2
    exit 1
fi
printf "clearautofill:\n" > "$HOUSE_ROOT/#.desktop/network_browser_request.txt"