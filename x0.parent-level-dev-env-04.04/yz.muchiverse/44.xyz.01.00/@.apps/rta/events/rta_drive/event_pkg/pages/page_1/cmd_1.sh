#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/rta/ops/+x/rta_life_op.+x" drive "$D/@.apps/rta/rules.pdl" "$D/#.desktop/rta_life_ledger.txt" 'joe' 'car' "$D/@.apps/rta/ops/+x/rta_momentum_op.+x" "$D/@.apps/rta/road.grid" "$D/#.desktop/rta_mom_ledger.txt" "$D/#.desktop/rta_mom_events.txt"
