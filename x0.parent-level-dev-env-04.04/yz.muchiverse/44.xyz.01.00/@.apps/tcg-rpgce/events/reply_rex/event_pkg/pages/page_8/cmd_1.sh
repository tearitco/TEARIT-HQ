#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/tcg-rpgce/ops/+x/tcg_rpgce.+x" pet_says 'rex' 'Ta-ra-ta-ra' 'Ta-ra-ta-ra' "$RPG_OTHER"
