#!/bin/bash
# nb_write_delhist.sh — write "delhist:<display_index>" to the
# network-browser manager's request file. 2 explicit args (verb, index)
# + the renderer's own appended package_dir/house_root, argc=4,
# house_root is $4 - same real convention as nb_write_go.sh (verb + 1
# real arg).
set -e
if [ $# -lt 4 ]; then
    echo "nb_write_delhist.sh: unexpected argc ($#), expected 4" >&2
    exit 1
fi
IDX="$2"
HOUSE_ROOT="$4"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "nb_write_delhist.sh: bad house_root" >&2
    exit 1
fi
printf "delhist:%s\n" "$IDX" > "$HOUSE_ROOT/#.desktop/network_browser_request.txt"
