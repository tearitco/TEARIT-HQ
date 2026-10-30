#!/bin/sh
# Read-only check of editor rows already drive-doc-fixed.
# Does not open a board, does not write a relay, does not change the camera.
# Usage: sh editor-skip.sh
# Exit 0 when the closed checks still hold. Exit 1 when one failed.
# See ../DONE.md and ../PROOF-2026-10-29.md.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../../../../44.xyz.01.00" && pwd)"
APP="$HOUSE/@.apps/rpg-edit-hq"
UI="$APP/board/state/ui.txt"
WORLD="$APP/rpg-edit-hq-pchq/pieces/world_01/state.txt"
PROJ="$HOUSE/@.apps/piececraft-hq/ops/pchq_board_projector.c"
fail=0

say() { printf '%s\n' "$1"; }

need() {
  if [ -e "$1" ]; then
    say "ok  file $1"
  else
    say "FAIL missing $1"
    fail=1
  fi
}

need "$UI"
need "$PROJ"

if [ -f "$UI" ]; then
  for id in cdda-hq mcl-hq df-hq; do
    if grep -q "arg=$id" "$UI"; then
      say "ok  menu lists $id"
    else
      say "FAIL menu has no arg=$id"
      fail=1
    fi
  done
  # Same visible label on two book rows is the old RPG-Pet miss.
  labels=$(grep '_label=' "$UI" | grep -E 'f_[0-9]+_label=' | sort | uniq -d | wc -l)
  if [ "$labels" -eq 0 ]; then
    say "ok  book labels are distinct"
  else
    say "FAIL duplicate book labels"
    fail=1
  fi
  book=$(sed -n 's/^book_label=book://p' "$UI" | head -1)
  page=$(sed -n 's/^page_label=page://p' "$UI" | head -1)
  say "ok  bar ${book:-empty}/${page:-empty}"
fi

if [ -f "$WORLD" ] && [ -n "${book:-}" ]; then
  map=$(sed -n 's/^map_id=//p' "$WORLD" | head -1)
  if [ "$map" = "$book" ]; then
    say "ok  world map_id matches the bar"
  else
    say "FAIL world map_id=$map bar=$book"
    fail=1
  fi
else
  say "ok  no live world file, bar check skipped"
fi

if [ -f "$PROJ" ] && grep -q 'bv_screen_changed.txt' "$PROJ"; then
  say "ok  projector wakes the pane on a book or page change"
else
  say "FAIL projector does not wake bv_screen_changed.txt"
  fail=1
fi

voice="$APP/state/flag_voice.txt"
if [ -f "$voice" ]; then
  v=$(tr -d '[:space:]' < "$voice")
  if [ "$v" = 1 ]; then
    say "ok  voice flag is 1"
  else
    say "FAIL voice flag is $v"
    fail=1
  fi
fi

if [ "$fail" -eq 0 ]; then
  say "SKIP closed rows. Next work is the first row not in DONE.md."
  exit 0
fi
exit 1
