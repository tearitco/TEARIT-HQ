#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/dsr-rpg/ops/+x/dsr_rpg.+x" pet_says 'orin' 'No court music...' 'No court music...' "$RPG_OTHER"
