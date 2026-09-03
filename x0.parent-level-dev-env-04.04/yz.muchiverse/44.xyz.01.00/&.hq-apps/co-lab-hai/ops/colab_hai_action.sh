#!/bin/bash
# colab_hai_action.sh — write one of "approve:"/"reject:"/"post:<msg>"
# to co-lab-hai's request file. Two real invocation shapes, matching
# network_browser_manager.c's own nb_write_go.sh convention exactly:
#   <item action="'.../colab_hai_action.sh' 'approve'"/>
#     -> the generic renderer appends <package_dir> <house_root>
#     -> real argv: colab_hai_action.sh approve <package_dir> <house_root>  (argc=3)
#   <cli_io action="'.../colab_hai_action.sh' 'post'"/>
#     -> the generic renderer appends <package_dir> <house_root> <typed_value>
#     -> real argv: colab_hai_action.sh post <package_dir> <house_root> <typed_value>  (argc=4)
set -e

VERB="$1"
if [ "$VERB" = "post" ]; then
    if [ $# -lt 4 ]; then
        echo "colab_hai_action.sh: post needs 4 args, got $#" >&2
        exit 1
    fi
    HOUSE_ROOT="$3"
    MSG="$4"
    if [ -z "$MSG" ]; then
        # empty submit - nothing to post, not an error
        exit 0
    fi
    REQ_LINE="post:$MSG"
else
    if [ $# -lt 3 ]; then
        echo "colab_hai_action.sh: $VERB needs 3 args, got $#" >&2
        exit 1
    fi
    HOUSE_ROOT="$3"
    REQ_LINE="$VERB:"
fi

if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "colab_hai_action.sh: bad house_root" >&2
    exit 1
fi
printf "%s\n" "$REQ_LINE" > "$HOUSE_ROOT/#.desktop/colab_hai/request.txt"
