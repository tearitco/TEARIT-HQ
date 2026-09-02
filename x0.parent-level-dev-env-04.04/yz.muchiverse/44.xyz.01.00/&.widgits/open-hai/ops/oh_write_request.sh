#!/bin/sh
# oh_write_request.sh — real, generic action script for open-hai's own
# real khtpm-shared-renderer .chtpm projection (2026-08-31, xperiments/
# khtpm-generic-dispatch-design.md's own capability #1/#2 work).
#
# Every non-cli_io <item action="..."> the projection writes embeds its
# own literal request line AND the manager's own real, live g_state_dir
# right into the action string itself, e.g.
#   action="&quot;.../oh_write_request.sh 'NEWSESSION' '/real/state/dir'&quot;"
# so that when the shared renderer's real, generic dispatch() runs it as
#   <action> '<package_dir>' '<house_root>'
# (see khtpm_core_render.c's own dispatch()) this script sees:
#   $1 = the real request line (NEWSESSION / LOADSESSION|<dir> /
#        DELETESESSION|<dir> / APPROVE / DENY / SEND|<escaped prompt>)
#   $2 = the real state dir to write request.txt into - taken directly
#        from write_chtpm_projection()'s own g_state_dir, NOT re-derived
#        from house_root here, so this keeps working correctly under
#        the manager's own real --data-root feature (a second instance
#        of this same binary running a persona pal's own separate
#        sessions/state, see khtpm_open_hai_manager.c's own main()
#        header comment) without this script needing to know that
#        feature exists at all.
#   $3 = package_dir (unused)
#   $4 = house_root (unused)
#
# request.txt is a real, single-line mailbox (khtpm_open_hai_manager.c's
# own handle_request() reads+clears one line per poll) - a real,
# deliberate OVERWRITE, not an append, matching khtpm_open_hai_render.c's
# own write_request() exactly (same file, same "one pending line" real
# contract, same reasoning as its own 2026-08-16 fix comment for
# DELETESESSION+NEWSESSION racing on this exact file).
LINE="$1"
STATE_DIR="$2"
printf '%s\n' "$LINE" > "$STATE_DIR/request.txt"
