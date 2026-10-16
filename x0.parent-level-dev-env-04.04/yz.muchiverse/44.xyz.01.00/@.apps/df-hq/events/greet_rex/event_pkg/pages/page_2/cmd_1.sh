#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/df-hq/ops/+x/df_hq.+x" pet_says 'rex' 'Greetings, {other}. The gate is quiet' 'Greetings, {other}. The gate is quiet' "$RPG_OTHER"
