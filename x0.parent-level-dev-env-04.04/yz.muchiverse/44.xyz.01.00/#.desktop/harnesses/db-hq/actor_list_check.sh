#!/bin/sh
# Confirm the Database window's projector is listing the desk actors.
# Registration itself is a write to actors.pdl / db_hq_actors.state.txt.
# dashboard.xhtpm has no "add actor" control, and class="db-hq-pal"
# keeps the old db_hq_history.txt path dormant. Keys for THIS window
# go to #.desktop/entity_menu_history/<renderer-pid>.txt as
# KEY_PRESSED lines (k9). Down-arrow moves the tab strip when focus
# is there: eight Downs plus Enter on 2026-09-23 opened the States
# tab instead of selecting Asa. Restore Actors with dbhq_action.sh
# tab, which is the same command the Actors tab click runs.
set -u
HOUSE="${HOUSE:-$(cd "$(dirname "$0")/../.." && pwd)}"
UI="$HOUSE/&.hq-apps/db-hq-pal/state/ui.txt"
[ -f "$UI" ] || { echo "no ui.txt — launch &.hq-apps/db-hq-pal/button.sh first" >&2; exit 1; }
missing=0
for name in Ember Glacine Murmur Solvent Asa Ava Cursword; do
  if grep -q "row_.*_text=.*$name" "$UI"; then
    echo "listed: $name"
  else
    echo "missing: $name" >&2
    missing=1
  fi
done
exit "$missing"
