#!/bin/bash
# nb_write_back.sh — write "back:" to the network-browser manager's
# request file. Toolbar <item action="'.../nb_write_back.sh' 'back'"/>
# - the generic renderer appends <package_dir> <house_root>, so the
# real invocation is: nb_write_back.sh back <package_dir> <house_root>
# (argc=3, house_root is $3), same convention nb_write_go.sh documents.
set -e
if [ $# -lt 3 ]; then
    echo "nb_write_back.sh: unexpected argc ($#), expected 3" >&2
    exit 1
fi
HOUSE_ROOT="$3"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "nb_write_back.sh: bad house_root" >&2
    exit 1
fi
printf "back:\n" > "$HOUSE_ROOT/#.desktop/network_browser_request.txt"
