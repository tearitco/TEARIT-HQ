#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/gta-hq/ops/+x/gta_hq.+x" pet_says 'maren' '{other}, you too, darling' '{other}, you too, darling' "$RPG_OTHER"
