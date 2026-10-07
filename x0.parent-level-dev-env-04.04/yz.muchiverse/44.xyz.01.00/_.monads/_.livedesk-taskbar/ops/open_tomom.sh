#!/bin/sh
# open_tomom.sh - HQ menu "tomom" row entry point. Thin wrapper under a
# glob-safe path (the app lives at &.hq-apps/tomom-hq/ and a leading '&' in
# an `sh -c` .pdl row is job-control). Pattern: open_mon.sh.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# ops -> _.livedesk-taskbar -> _.monads -> house_root
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
exec sh "$HOUSE_ROOT/&.hq-apps/tomom-hq/open_tomom_hq.sh" "$HOUSE_ROOT"
