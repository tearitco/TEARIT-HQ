#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/civ-rpgce/ops/+x/civ_rpgce.+x" pet_says 'gus' 'No music for the pot...' 'No music for the pot...' "$RPG_OTHER"
