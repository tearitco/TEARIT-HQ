#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/rpg-pvp-hq/ops/+x/rpg_pvp_hq.+x" pet_says 'pip' 'No joke books here.' 'No joke books here.' "$RPG_OTHER"
