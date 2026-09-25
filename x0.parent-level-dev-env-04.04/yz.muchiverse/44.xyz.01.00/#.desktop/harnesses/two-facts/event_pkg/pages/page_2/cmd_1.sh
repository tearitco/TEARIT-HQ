#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec sh "$D/#.desktop/harnesses/two-facts/apply_range.sh" 'a1' 'b2' '2' '6' "$D/#.desktop/db_hq_actors.state.txt" "$D/&.widgits/db-hq/data/actors.pdl"
