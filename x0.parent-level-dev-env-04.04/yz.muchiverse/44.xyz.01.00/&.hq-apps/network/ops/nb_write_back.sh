#!/bin/bash
# nb_write_back.sh — write "back:" into the manager request file.
# Renderer appends <package_dir> <house_root>; house_root is the last argument.
set -e
if [ $# -lt 1 ]; then
    echo "nb_write_back.sh: missing argv" >&2
    exit 1
fi
HOUSE_ROOT="${@: -1}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "nb_write_back.sh: bad house_root" >&2
    exit 1
fi
mkdir -p "$HOUSE_ROOT/#.desktop"
printf "%s\n" "back:" > "$HOUSE_ROOT/#.desktop/network_browser_request.txt"
