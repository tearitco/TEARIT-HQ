#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/mcl-hq/ops/+x/mcl_hq.+x" pet_says 'pip' '{other}! Watch this trick' '{other}! Watch this trick' "$RPG_OTHER"
