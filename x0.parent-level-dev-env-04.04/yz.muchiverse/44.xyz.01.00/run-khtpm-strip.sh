#!/bin/sh
# run-khtpm-strip.sh — house-root-level starter for the new khtpm strip
# parser/manager pair (the 2026-08-11 taskbar refactor). Delegates to the
# real script at *.monads/*.livedesk-taskbar/ops/run_khtpm_strip.sh — see
# that file's own header comment, and
# #.#.calendar-dox/aug-11-refactor-finish.md, for the full story.
set -e
HOUSE_DIR="$(cd "$(dirname "$0")" && pwd)"
exec sh "$HOUSE_DIR/*.monads/*.livedesk-taskbar/ops/run_khtpm_strip.sh" "$@"
