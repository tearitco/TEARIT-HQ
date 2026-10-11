#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/rpg-pet/ops/+x/rpg_pet.+x" pet_says 'fang' 'I have nowhere to eat.' '我没地方吃饭。' "$RPG_OTHER"
