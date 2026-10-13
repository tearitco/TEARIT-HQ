#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/cdda-hq/ops/+x/cdda_hq.+x" pet_says 'orin' 'No law books here.' 'No law books here.' "$RPG_OTHER"
