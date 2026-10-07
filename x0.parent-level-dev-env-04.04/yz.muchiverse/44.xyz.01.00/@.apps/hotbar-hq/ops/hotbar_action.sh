#!/bin/sh
# hotbar_action.sh <slot-index 0-based> <state_dir> - select a hotbar slot in the
# current holder's inventory (the manager republishes within ~0.4 s).
HOUSE="$(cd "$(dirname "$0")/../../.." && pwd)"
IOP="$HOUSE/&.widgits/entity-cli/ops/+x/inventory_op.+x"
H="$(cat "$2/holder.txt" 2>/dev/null)"
[ -n "$H" ] && [ -x "$IOP" ] && "$IOP" slot "$H" "$1" >/dev/null 2>&1
exit 0
