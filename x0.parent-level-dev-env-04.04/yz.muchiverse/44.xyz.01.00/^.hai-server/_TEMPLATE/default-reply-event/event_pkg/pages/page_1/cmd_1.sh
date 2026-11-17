#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/&.widgits/_shared-lib/ops/+x/phone_send_op.+x" "$MUCHI_PHONE" "$MUCHI_MSG_FROM" say "t:$MUCHI_MSG_FROM" '[auto] automatic reply: I got your message and will answer later'
