#!/bin/bash
# nb_write_reload.sh — write "reload:" into the manager request file.
set -e
if [ $# -lt 1 ]; then
    echo "nb_write_reload.sh: missing argv" >&2
    exit 1
fi
HOUSE_ROOT="${@: -1}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "nb_write_reload.sh: bad house_root" >&2
    exit 1
fi
mkdir -p "$HOUSE_ROOT/#.desktop"
printf "%s\n" "reload:" > "$HOUSE_ROOT/#.desktop/network_browser_request.txt"
