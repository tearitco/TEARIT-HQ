#!/bin/sh
# mr_scrolling_text.+x takes house_root and text as literal argv (see
# its own header comment: "a real, generic non-blocking popup" - no
# text-file indirection needed, unlike mr_show_text above).
PAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PAGE_DIR/../../.." || exit 1
ENT="$PWD"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/&.widgits/events-hq/ops/+x/mr_scrolling_text.+x" "$ENT" "$D" "...but the words have faded with time."
