#!/bin/sh
# open_tpmos.sh - HQ menu "tpmos" row entry point. Thin wrapper under a glob-safe path (the app lives at @.apps/tpmos-hq/). Pattern: open_tomom.sh.
# Starts the real TPMOS (its own button.sh run) if needed and opens the x11-hq window on its start screen; detached, so the menu returns at once.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
exec sh "$HOUSE_ROOT/@.apps/tpmos-hq/button.sh" run
