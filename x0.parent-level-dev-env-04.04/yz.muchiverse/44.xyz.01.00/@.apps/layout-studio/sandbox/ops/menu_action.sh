#!/bin/sh
# Sandbox test-menu actions: append a line to state/actions.log (proof an item ran).
D="$(cd "$(dirname "$0")/.." && pwd)/state"
printf '%s %s\n' "$(date +%T)" "$1" >> "$D/actions.log"
