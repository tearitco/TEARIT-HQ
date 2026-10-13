#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/mcl-hq/ops/+x/mcl_hq.+x" pet_says 'fenn' 'I like fairy tales.' 'I like fairy tales.' "$RPG_OTHER"
