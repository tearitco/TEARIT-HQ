#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/&.widgits/world-ops/ops/+x/wo.+x" if "$(dirname "$(dirname "$ENT")")" ':speed:' 'ge' ':env_breach_speed:' 'col_breach'
