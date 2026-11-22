#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/vox/ops/+x/vox_op.+x" mine "$D/@.apps/vox" "$D/#.desktop/vox_ledger.txt" 'joe' 'pick' '3,2,3'
