#!/bin/bash
# nb_write_newtab.sh — write "newtab:" to the network-browser manager's
# request file. Same argv convention as nb_write_back.sh (argc=3,
# house_root is $3). The manager already handles this verb
# (handle_request()'s own "newtab:"/"newtab" branch) - this script was
# simply missing from the merge, so clicking "New tab" silently failed
# (system() on a nonexistent script, no visible error).
set -e
if [ $# -lt 3 ]; then
    echo "nb_write_newtab.sh: unexpected argc ($#), expected 3" >&2
    exit 1
fi
HOUSE_ROOT="$3"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "nb_write_newtab.sh: bad house_root" >&2
    exit 1
fi
printf "newtab:\n" > "$HOUSE_ROOT/#.desktop/network_browser_request.txt"
