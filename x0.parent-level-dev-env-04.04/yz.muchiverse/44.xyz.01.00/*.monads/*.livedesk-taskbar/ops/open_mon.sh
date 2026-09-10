#!/bin/sh
# open_mon.sh - HQ menu "mon" row entry point. Thin wrapper (kept here,
# under a glob-safe path, because the HQ menu dispatch runs the cmd via
# `sh -c` and the real target lives at &.hq-apps/mon-hq/ - a leading '&'
# is shell job-control, so it can't be named directly in the .pdl row).
# Mirrors open_cli.sh: resolve this house root from $0, exec the app.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# ops -> *.livedesk-taskbar -> *.monads -> house_root
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
exec sh "$HOUSE_ROOT/&.hq-apps/mon-hq/button.sh" "$HOUSE_ROOT"
