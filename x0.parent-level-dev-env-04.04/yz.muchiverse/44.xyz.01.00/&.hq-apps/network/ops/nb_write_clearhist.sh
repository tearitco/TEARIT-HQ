#!/bin/bash
# nb_write_clearhist.sh — write "clearhist:" to the network-browser
# manager's request file. Same argv convention as nb_write_bookmark.sh
# (1 explicit arg, argc=3, house_root is $3).
set -e
if [ $# -lt 3 ]; then
    echo "nb_write_clearhist.sh: unexpected argc ($#), expected 3" >&2
    exit 1
fi
HOUSE_ROOT="$3"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "nb_write_clearhist.sh: bad house_root" >&2
    exit 1
fi
printf "clearhist:\n" > "$HOUSE_ROOT/#.desktop/network_browser_request.txt"
