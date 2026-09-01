#!/bin/sh
# ch_send.sh — real, generic action script for chat-hai's own
# generic .chtpm projection composer field (2026-09-01 migration onto
# the shared khtpm_core_render.+x, see chat_hai_projector.sh's own
# header comment). This is a <cli_io>'s own action=, so the shared
# renderer's real default_cli_io_run_action() invokes it as
#   <action> '<package_dir>' '<house_root>' '<live typed value>'
# (see khtpm_core_render.c) - the projection bakes NO literal argv of
# its own, so:
#   $1 = package_dir (unused)
#   $2 = house_root
#   $3 = the composer's real, live typed text at the moment Enter fired
#
# Appends directly to the ACTIVE session's own ledger, in the exact
# same real line format the old chai_send_composer() (now-dead C code)
# used to write by hand - chat_hai_loop.sh's own ledger reader has no
# idea this text came from a different writer.
HOUSE_ROOT="$2"
TEXT="$3"
[ -z "$TEXT" ] && exit 0

STATE_DIR="$HOUSE_ROOT/&.hq-apps/chat-hai/state"
SESSIONS_DIR="$STATE_DIR/sessions"
ACTIVE_FILE="$SESSIONS_DIR/active.txt"
mkdir -p "$SESSIONS_DIR"
ACTIVE="$(cat "$ACTIVE_FILE" 2>/dev/null || echo main)"
LEDGER="$SESSIONS_DIR/$ACTIVE.ledger"
TS="$(date '+%Y-%m-%d %H:%M:%S')"
printf '[%s] user: %s | Trigger: chat-hai\n' "$TS" "$TEXT" >> "$LEDGER"
