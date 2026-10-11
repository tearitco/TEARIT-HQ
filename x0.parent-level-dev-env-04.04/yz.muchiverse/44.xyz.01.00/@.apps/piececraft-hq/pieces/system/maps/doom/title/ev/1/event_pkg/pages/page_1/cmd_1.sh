#!/bin/sh
D="$(cd "$(dirname "$0")" && pwd)"
while [ "$D" != / ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
sh "$D/@.apps/piececraft-hq/ops/doom_event.sh" start_game
