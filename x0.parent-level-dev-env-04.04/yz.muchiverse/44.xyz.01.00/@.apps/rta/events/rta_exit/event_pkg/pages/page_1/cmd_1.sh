#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/rta/ops/+x/rta_life_op.+x" exit "$D/@.apps/rta/rules.pdl" "$D/#.desktop/rta_life_ledger.txt" 'joe' 'car'
