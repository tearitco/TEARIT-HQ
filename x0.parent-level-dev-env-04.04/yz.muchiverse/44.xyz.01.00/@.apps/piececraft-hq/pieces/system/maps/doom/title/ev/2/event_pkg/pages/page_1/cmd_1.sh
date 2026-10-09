#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
D="$PWD"
while [ "$D" != / ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/@.apps/piececraft-hq/ops/doom_event.sh" stop_game
