#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="$PWD"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/*.monads/*.muchi-pet/ops/+x/mr_show_choices.+x" "$ENT" 'Say hello,Wave,Ignore' 0
