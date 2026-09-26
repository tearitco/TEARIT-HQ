#!/bin/sh

# button_taskbar_settings.sh — launch the taskbar HQ menu's "Settings"
# window. 2026-09-04: retired the old taskbar_settings.chtpm /
# g_is_swatch_picker path entirely (dead weight competing with this
# one - see #.#.calendar-dox handoff for the incident). This is now a
# thin, permanent redirect to the real, static-xhtpm implementation.
# Usage: button_taskbar_settings.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "taskbar-settings button.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

_TBP="$HOUSE_ROOT/&.widgits/taskbar-settings/button-pal.sh"
if [ ! -f "$_TBP" ]; then
    echo "taskbar-settings button.sh: missing $_TBP" >&2
    exit 1
fi
exec sh "$_TBP" "$HOUSE_ROOT"
