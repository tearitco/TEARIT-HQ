#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/rpg-pet/ops/+x/rpg_pet.+x" pet_says 'blip' 'no piano...' 'ピアノがないなあ…' "$RPG_OTHER"
