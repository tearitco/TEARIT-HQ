#!/bin/sh
# pc_synch_request.sh taskbar|pchq
# Records that Player > Synch was pressed. Does not copy a book or page.
# The copy is still an open question. See
# !.HQ-IQ-BOOK/09-appendix/PC-HQ-BOOK-PAGE-SYNCH.md
SELF=$(cd "$(dirname "$0")" && pwd)
HOUSE=$(cd "$SELF/../../.." && pwd)
FROM=${1:-unknown}
DESK="$HOUSE/#.desktop"
mkdir -p "$DESK"
printf 'from=%s\ntime=%s\nstatus=row-only\n' "$FROM" "$(date +%H:%M:%S)" \
  > "$DESK/pc_synch_request.txt"
