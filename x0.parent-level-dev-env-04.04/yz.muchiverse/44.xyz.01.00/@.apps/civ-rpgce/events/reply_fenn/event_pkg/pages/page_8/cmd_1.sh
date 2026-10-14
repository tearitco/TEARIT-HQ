#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/civ-rpgce/ops/+x/civ_rpgce.+x" pet_says 'fenn' 'La la la, tinkle' 'La la la, tinkle' "$RPG_OTHER"
