#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/df-hq/ops/+x/df_hq.+x" pet_says 'rex' 'I study the maps.' 'I study the maps.' "$RPG_OTHER"
