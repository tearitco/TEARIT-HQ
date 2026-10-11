#!/bin/sh
# button.sh - the Toys launcher (the taskbar runs `sh button.sh run`). v0 is headless: it asks the conductor for its Status through the same event page the context menu uses.
D=$(cd "$(dirname "$0")" && pwd); HOUSE=$(cd "$D/../../.." && pwd)
exec "$HOUSE/&.widgits/digipet/ops/+x/event_page_op.+x" "$D" "$HOUSE" --trigger status
