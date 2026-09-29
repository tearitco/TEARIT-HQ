#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/&.widgits/events-hq/ops/+x/mr_read_receipt.+x" "$ENT" "$D" '/tmp/rr.receipt.pdl' 'focus_nav' 'focus_nav' 'on_harold=17'
