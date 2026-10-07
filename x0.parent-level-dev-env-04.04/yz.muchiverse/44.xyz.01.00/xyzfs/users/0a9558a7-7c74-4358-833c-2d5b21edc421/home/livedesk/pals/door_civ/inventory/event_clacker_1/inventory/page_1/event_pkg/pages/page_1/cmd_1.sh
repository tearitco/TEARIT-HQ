#!/bin/sh
# Real "transfer player" Common Event - teleports the player from
# whatever desk this door is on to civ-test, via the new, real,
# self-contained mr_transfer_desk.+x op (see that file's own header
# comment for the resolved architecture decision: it duplicates
# livedesk_switch_desk()'s own real mechanics rather than polling the
# running manager - it's just another op reached through the same
# play_event.sh -> prisc+x -> op path every other event command uses).
cd "$(dirname "$0")/../../.." || exit 1
ENT="$PWD"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
HOUSE_ROOT="$D"

# Resolve the real active session id the same way every other real
# consumer of session.pdl does (STATE | active_session | <id>).
UUID_DIR="$(dirname "$(dirname "$(dirname "$ENT")")")"
SESS_ROOT="$UUID_DIR/livedesk/sessions"
SESSION_ID="$(awk -F'|' '/active_session/{gsub(/ /,"",$3); print $3}' "$SESS_ROOT/session.pdl")"
[ -n "$SESSION_ID" ] || SESSION_ID=s1

BIN="$HOUSE_ROOT/&.widgits/events-hq/ops/+x/mr_transfer_desk.+x"
exec "$BIN" "$HOUSE_ROOT" "$SESSION_ID" "civ-test"
