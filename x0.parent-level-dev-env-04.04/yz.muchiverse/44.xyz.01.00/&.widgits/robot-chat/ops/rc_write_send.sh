#!/bin/sh
# rc_write_send.sh - real, generic action script for robot-chat.xhtpm's
# own composer <cli_io>. Same real mechanism oh_write_send.sh already
# uses (default_cli_io_run_action() invokes any cli_io action= as
# `<action> '<dir>' '<house_root>' '<live typed value>'`) - but SIMPLER
# than open-hai's version because this window is entity-scoped
# (argv[3]=entity_dir at launch, khtpm_core_render.c's g_arg3_dir hook):
# after 2026-09-28's fix, <dir> here is ALREADY the real per-entity dir
# (g_arg3_dir), not the shared &.widgits/robot-chat/ template dir - no
# need to bake a state-dir path into the action= string like open-hai
# has to (open-hai is single-instance, never uses g_arg3_dir).
#   $1 = the real entity dir (g_arg3_dir)
#   $2 = house_root (unused)
#   $3 = the composer's real, live typed text at the moment Enter fired
#
# Encodes like khtpm_open_hai_manager.c's own escape_line() - backslash
# doubled - so khtpm_robot_chat_manager.+x's own unescape decodes it
# back byte for byte. A cli_io field can never hold a literal newline
# (Enter always submits), so \n is never a real case here.
ENTITY_DIR="$1"
TEXT="$3"
mkdir -p "$ENTITY_DIR/.hq_manager"
ESCAPED=$(printf '%s' "$TEXT" | sed -e 's/\\/\\\\/g')
printf 'SEND|%s\n' "$ESCAPED" > "$ENTITY_DIR/.hq_manager/request.txt"
