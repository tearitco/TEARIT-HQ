#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="${MUCHI_TARGET_ENT:-$PWD}"
SCRIPT="inventory"
D="$ENT"
HOUSE_ROOT=""
while [ "$D" != "/" ]; do
if [ -d "$D/xyzfs" ]; then HOUSE_ROOT="$D"; break; fi
D="$(dirname "$D")"
done
if [ -z "$HOUSE_ROOT" ]; then
D="$ENT"
while [ "$D" != "/" ]; do
if [ -d "$D/yz.muchiverse" ]; then HOUSE_ROOT="$D"; break; fi
D="$(dirname "$D")"
done
fi
[ -n "$HOUSE_ROOT" ] || { echo "df_event: cannot find house root" >&2; exit 1; }
exec "$HOUSE_ROOT/yz.muchiverse/44.xyz.01.00/&.widgits/events-hq/ops/df_event.sh" "$ENT" "$SCRIPT"
