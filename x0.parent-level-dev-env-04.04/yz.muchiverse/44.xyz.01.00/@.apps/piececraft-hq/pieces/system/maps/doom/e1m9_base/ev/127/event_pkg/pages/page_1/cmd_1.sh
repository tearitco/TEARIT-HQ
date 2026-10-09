#!/bin/sh
D="$(cd "$(dirname "$0")" && pwd)"
while [ "$D" != / ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
sh "$D/@.apps/piececraft-hq/ops/doom_event.sh" change_ammo
sh "$D/@.apps/piececraft-hq/ops/doom_event.sh" kill_event e1m9_base 127
