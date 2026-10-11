#!/bin/sh
# pet_clock_runner.sh <pkg> <root> - what the pet's clock daemon runs for a reminder (LC_CLOCK_EVENT_RUNNER). pkg ends in the event name (common:<event> -> <root>/common_events/<event>).
HERE="$(cd "$(dirname "$0")/.." && pwd)"; EV="$(basename "$1")"
PET_SHARED="${PET_SHARED:-$HERE/state}" PET_DIR= exec sh "$HERE/ops/pet_event.sh" clock_event "$EV"
