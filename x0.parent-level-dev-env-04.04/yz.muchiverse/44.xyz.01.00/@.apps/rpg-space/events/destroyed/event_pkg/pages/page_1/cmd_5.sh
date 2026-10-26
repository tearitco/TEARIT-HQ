#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/&.widgits/world-ops/ops/+x/wo.+x" event "$(dirname "$(dirname "$ENT")")" 'DESTROYED' ':kind:/:room:,:x:,:y:/:what:/by :src:' '' '' ''
