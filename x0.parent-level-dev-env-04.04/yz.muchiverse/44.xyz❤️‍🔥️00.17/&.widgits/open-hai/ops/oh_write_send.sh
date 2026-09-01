#!/bin/sh
# oh_write_send.sh — real, generic action script for open-hai's own real
# .chtpm projection composer field (2026-08-31, xperiments/khtpm-
# generic-dispatch-design.md capabilities #1/#2). This is a <cli_io>'s
# own action=, so the shared renderer's real default_cli_io_run_action()
# invokes it as
#   <action> '<package_dir>' '<house_root>' '<live typed value>'
# (see khtpm_core_render.c) - a DIFFERENT real argv order than a plain
# <item>'s action= (see oh_write_request.sh's own header comment for
# that one) - this is the cli_io-specific shape, not a copy/paste bug.
# The projection embeds the manager's own real, live g_state_dir as
# this action's own literal argv, same real reason oh_write_request.sh
# takes it as an argv too (keeps this correct under the manager's own
# --data-root feature without this script needing to know it exists):
#   $1 = the real state dir to write request.txt into
#   $2 = package_dir (unused)
#   $3 = house_root (unused)
#   $4 = the composer's real, live typed text at the moment Enter fired
#
# Encodes exactly like khtpm_open_hai_manager.c's own escape_line() -
# backslash doubled - so its own unescape_line() in handle_request()'s
# SEND| branch decodes it back to the exact original text, byte for
# byte. A real cli_io field can never contain a literal newline (Enter
# always submits immediately - see default_cli_io_handle_key()), so the
# reference escape_line()'s own \n case never applies here.
STATE_DIR="$1"
TEXT="$4"
ESCAPED=$(printf '%s' "$TEXT" | sed -e 's/\\/\\\\/g')
printf 'SEND|%s\n' "$ESCAPED" > "$STATE_DIR/request.txt"
