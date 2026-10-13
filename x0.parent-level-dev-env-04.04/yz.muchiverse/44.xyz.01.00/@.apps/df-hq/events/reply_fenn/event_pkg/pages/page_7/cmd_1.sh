#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/df-hq/ops/+x/df_hq.+x" pet_says 'fenn' 'No chimes...' 'No chimes...' "$RPG_OTHER"
