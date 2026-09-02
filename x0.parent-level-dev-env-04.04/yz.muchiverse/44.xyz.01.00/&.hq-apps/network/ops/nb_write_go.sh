#!/bin/bash
# nb_write_go.sh — write a "go:<url>" request to the network-browser
# manager's own request file. Invoked by the renderer as an <item>
# action (link clicked) or <cli_io> action (address bar Enter).
#
# REAL FIX 2026-09-01 (found + fixed live, direct verification against
# khtpm_core_render.c's own real, fixed dispatch conventions): the
# original version of this script assumed a DIFFERENT argv shape than
# either real convention actually uses, so it silently wrote nothing
# (or wrote to the wrong #.desktop dir) on every real invocation. The
# manager's own write_chtpm_projection() bakes a literal
# "'<script>' 'go' '<X>'" into each action= string; the generic
# renderer then APPENDS its own fixed trailing args on top of that:
#   <item action>:   ... appends  <package_dir> <house_root>
#   <cli_io action>: ... appends  <package_dir> <house_root> <typed_value>
# So the two real, full invocations are:
#   item:   nb_write_go.sh go <url>          <package_dir> <house_root>
#           $1=go  $2=url            $3=package_dir $4=house_root
#   cli_io: nb_write_go.sh go <chtpm_path> <package_dir> <house_root> <typed_value>
#           $1=go  $2=chtpm_path(unused) $3=package_dir $4=house_root $5=typed_value
# Distinguished by argc (4 vs 5); house_root is $4 in both cases.
set -e

if [ $# -eq 5 ]; then
    # <cli_io> address-bar submit - real URL is the live typed value ($5)
    HOUSE_ROOT="$4"
    URL="$5"
elif [ $# -eq 4 ]; then
    # <item> link click - real URL is $2
    HOUSE_ROOT="$4"
    URL="$2"
else
    echo "nb_write_go.sh: unexpected argc ($#), expected 4 (item) or 5 (cli_io)" >&2
    exit 1
fi

if [ -z "$HOUSE_ROOT" ] || [ -z "$URL" ]; then
    echo "nb_write_go.sh: missing house_root or URL (argc=$#)" >&2
    exit 1
fi

DESKTOP_DIR="$HOUSE_ROOT/#.desktop"
REQUEST_FILE="$DESKTOP_DIR/network_browser_request.txt"
printf "go:%s\n" "$URL" > "$REQUEST_FILE"
