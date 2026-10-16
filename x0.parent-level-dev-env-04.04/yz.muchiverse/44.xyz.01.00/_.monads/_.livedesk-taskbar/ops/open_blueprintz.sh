#!/bin/sh
# the db dropdown's "blueprintz" row (livedesk_taskbar.pdl db_menu_4_cmd). No & in this path, so the generic `sh <path>` menu command is safe; same shape as open_knowledge.sh.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
exec sh "$HOUSE_ROOT/&.hq-apps/blueprintz/button.sh" "$HOUSE_ROOT"
