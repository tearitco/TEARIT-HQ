#!/bin/bash
# colab_hai_action.sh — write one request line to co-lab-hai's request
# file. Real invocation shapes, matching network_browser_manager.c's
# own nb_write_go.sh convention exactly (an <item>'s action= gets
# <package_dir> <house_root> appended by the generic renderer; a
# <cli_io>'s also gets the real typed value appended after that):
#   <item action="'.../colab_hai_action.sh' 'approve'"/>
#     -> approve <package_dir> <house_root>                       (argc=3)
#   <item action="'.../colab_hai_action.sh' 'newsession'"/>
#     -> newsession <package_dir> <house_root>                    (argc=3)
#   <item action="'.../colab_hai_action.sh' 'loadsession' '<sid>'"/>
#     -> loadsession <sid> <package_dir> <house_root>             (argc=4,
#        one extra literal arg ahead of house_root - session id)
#   <cli_io action="'.../colab_hai_action.sh' 'post'"/>
#     -> post <package_dir> <house_root> <typed_value>            (argc=4)
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
elif [ "$VERB" = "loadsession" ]; then
    if [ $# -lt 4 ]; then
        echo "colab_hai_action.sh: loadsession needs 4 args, got $#" >&2
        exit 1
    fi
    SID="$2"
    HOUSE_ROOT="$4"
    REQ_LINE="loadsession:$SID"
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
# REAL FIX 2026-09-29, direct live report ("it keeps asking for
# approval for an old message u sent that i appended a note to") -
# request.txt is now a real append-only queue (colab_hai_manager.c's
# handle_request() drains every queued line per tick, same shape as
# drain_incoming()), not a single-slot mailbox. It used to be `>`
# (whole-file overwrite) - clicking Approve then submitting a composer
# note before the manager's next poll tick silently clobbered the
# still-unread approve line, so it never actually cleared.
printf "%s\n" "$REQ_LINE" >> "$HOUSE_ROOT/#.desktop/colab_hai/request.txt"
