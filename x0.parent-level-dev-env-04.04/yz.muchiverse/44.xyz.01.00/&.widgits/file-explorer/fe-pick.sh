#!/bin/sh
# fe-pick.sh <LOAD|SAVE> <start_dir> [timeout_s] - modal file pick.
#
# Writes the fe_request.txt contract, launches the File Explorer widget,
# waits for the user to pick (or the window to close), and prints the
# chosen absolute path to stdout (empty line = cancelled / timed out).
#
#   PICK="$(sh "$HOUSE/&.widgits/file-explorer/fe-pick.sh" LOAD "$somedir")"
#   [ -n "$PICK" ] && do_something_with "$PICK"
#
# Callers: strip file cell "load", pc-hq board File menu, any future
# "browse to a file and act on it" flow. One pick at a time (modal).
set -u
MODE="${1:-LOAD}"
START="${2:-}"
TIMEOUT="${3:-180}"

FE_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$FE_DIR/../.." && pwd)"
[ -d "$START" ] || START="$HOUSE"

REQ="$FE_DIR/fe_request.txt"
RES="$(mktemp "${TMPDIR:-/tmp}/fe_pick.XXXXXX")"

{
  echo "mode=$MODE"
  echo "start_dir=$START"
  echo "result_file=$RES"
} > "$REQ"
: > "$RES"

# launch detached; button.sh renders the widget + spawns the manager,
# which consumes fe_request.txt on startup.
setsid sh "$FE_DIR/button.sh" run >/dev/null 2>&1 < /dev/null &

# wait for the manager to write RES (non-empty) or for the widget to
# close (no khtpm_core_render on file-explorer-pal.xhtpm) or timeout.
i=0
steps=$(( TIMEOUT * 5 ))
while [ "$i" -lt "$steps" ]; do
    if [ -s "$RES" ]; then break; fi
    if ! pgrep -f "khtpm_core_render[.][+]x .*file-explorer-pal[.]xhtpm" >/dev/null 2>&1 \
       && [ "$i" -gt 10 ]; then
        break
    fi
    sleep 0.2
    i=$(( i + 1 ))
done

PICK=""
[ -s "$RES" ] && PICK="$(head -n1 "$RES")"
rm -f "$RES" "$REQ"
printf '%s\n' "$PICK"
