#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/rpg-edit-hq/ops/+x/rpg_edit_hq.+x" pet_says 'ember' '@plug:day-night:greeting_chat' '@plug:day-night:greeting_voice' "$RPG_OTHER"
