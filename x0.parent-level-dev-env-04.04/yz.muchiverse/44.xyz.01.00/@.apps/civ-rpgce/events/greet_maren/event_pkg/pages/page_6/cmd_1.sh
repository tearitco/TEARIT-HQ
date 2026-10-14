#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/civ-rpgce/ops/+x/civ_rpgce.+x" pet_says 'maren' 'Hello again, {other}' 'Hello again, {other}' "$RPG_OTHER"
