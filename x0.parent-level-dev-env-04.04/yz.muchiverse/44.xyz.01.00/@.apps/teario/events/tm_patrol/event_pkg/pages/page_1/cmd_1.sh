#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/&.widgits/_shared-lib/ops/+x/rule_patrol_op.+x" "$D/@.apps/teario/events/tm_patrol/scene.pdl" '12'
