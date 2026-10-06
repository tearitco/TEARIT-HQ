#!/bin/bash
# nb_write_click.sh — write a "click:<selector>[:<type>]" request to the
# network-browser manager's own request file. Invoked by the renderer as an
# <item>/<text> action when a rendered content row is clicked.
#
# The manager turns that into an EVENT RPC to the QuickJS worker
# (worker_send_event), which resolves the selector with
# document.querySelector() and dispatches a real bubbling Event on it, then
# re-renders — so a page's own onclick/oninput handler runs and its result
# shows up in the projection without a reload.
#
# Same real argv contract as nb_write_go.sh: the renderer's generic dispatch
# appends <package_dir> <house_root> to an <item> action, so the full
# invocation is:
#   nb_write_click.sh click <selector> [type] <package_dir> <house_root>
#   $1=click  $2=selector  $3=type  $4=package_dir  $5=house_root
# argc 5 (type given) or 4 (defaults to "click").
set -e

if [ $# -eq 5 ]; then
    HOUSE_ROOT="$5"
    SELECTOR="$2"
    TYPE="$3"
elif [ $# -eq 4 ]; then
    HOUSE_ROOT="$4"
    SELECTOR="$2"
    TYPE="click"
else
    echo "nb_write_click.sh: unexpected argc ($#), expected 4 (item) or 5 (item+type)" >&2
    exit 1
fi

if [ -z "$HOUSE_ROOT" ] || [ -z "$SELECTOR" ]; then
    echo "nb_write_click.sh: missing house_root or selector (argc=$#)" >&2
    exit 1
fi

DESKTOP_DIR="$HOUSE_ROOT/#.desktop"
REQUEST_FILE="$DESKTOP_DIR/network_browser_request.txt"
printf "click:%s:%s\n" "$SELECTOR" "$TYPE" > "$REQUEST_FILE"
