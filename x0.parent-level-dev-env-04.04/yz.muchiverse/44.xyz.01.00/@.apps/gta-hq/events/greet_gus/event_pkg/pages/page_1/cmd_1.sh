#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/gta-hq/ops/+x/gta_hq.+x" pet_says 'gus' 'Hungry, {other}? Come and eat!' 'Hungry, {other}? Come and eat!' "$RPG_OTHER"
