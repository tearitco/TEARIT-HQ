#!/bin/sh
# mr_show_text.+x needs a real text FILE path (its own header comment,
# op's cwd is the muta system dir when run under prisc+x via
# play_event.sh, not this script's own dir) - resolve text_1.txt's
# absolute path BEFORE the cd below, same convention m8_redhorned's own
# cmd_4.sh should have used (that one happens to pass literal text as a
# filename, a real, separate, pre-existing bug not touched here).
PAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
TEXT_FILE="$PAGE_DIR/text_1.txt"
cd "$PAGE_DIR/../../.." || exit 1
ENT="$PWD"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/&.widgits/events-hq/ops/+x/mr_show_text.+x" "$ENT" "$TEXT_FILE"
