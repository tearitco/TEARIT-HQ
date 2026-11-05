#!/bin/bash
# nb_write_print.sh - ask the network-browser manager to print the
# current page to a PDF (into the app's own downloads/ dir). Chrome
# action wiring, identical contract to nb_write_go.sh: writes one line
# to the manager's request file and returns. The manager consumes it,
# generates the PDF and publishes the result as a synthetic page (or an
# error row) - this script itself does none of that work.
#
# Usage (from a chrome <item action="'...nb_write_print.sh' 'print'"/>):
#   nb_write_print.sh print <package_dir> <house_root>
set -e

HOUSE_ROOT="${3:-${2:-}}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "nb_write_print.sh: need house_root as argv[2 or 3] (got: $*)" >&2
    exit 1
fi

DESKTOP_DIR="$HOUSE_ROOT/#.desktop"
mkdir -p "$DESKTOP_DIR"
printf 'print:\n' > "$DESKTOP_DIR/network_browser_request.txt"
