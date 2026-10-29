#!/bin/sh
# trim_book_dbs.sh - turn each petslike game's FULL db copy (<app>/db) into an empty LAYER over the shared trunk (&.widgits/db-hq/data)
# (BOOK 16.game/PER-BOOK-DB-2026-10-28.md section 6). It removes nothing itself: it runs each game's own tested verb `db trim`, which drops ONLY
# layer files that are byte-identical to the trunk's (anything a book really changed stays). Default = DRY RUN (lists what is identical, changes nothing).
#   sh trim_book_dbs.sh          dry run
#   sh trim_book_dbs.sh --do     really trim
# After --do the files show as deleted in git: commit them with an explicit pathspec (git add -- <app>/db ; git commit -- <app>/db).
set -u
HERE=$(cd "$(dirname "$0")" && pwd); HOUSE=$(cd "$HERE/../../.." && pwd); TRUNK="$HOUSE/&.widgits/db-hq/data"
DO=0; [ "${1:-}" = "--do" ] && DO=1
[ -d "$TRUNK" ] || { echo "no trunk at $TRUNK" >&2; exit 2; }
total_before=0; total_after=0
for a in cdda-hq civ-rpgce df-hq dsr-rpg gta-hq mcl-hq rpg-dust rpg-edit-hq rpg-pet rpg-space tcg-rpgce; do
  app="$HOUSE/@.apps/$a"; bin="$app/ops/+x/$(echo "$a" | tr - _).+x"; db="$app/db"
  [ -d "$db" ] || { echo "$a: no db folder (skipped)"; continue; }
  [ -x "$bin" ] || { echo "$a: binary missing, build it first (skipped)"; continue; }
  files=0; same=0
  for f in "$db"/*; do
    [ -f "$f" ] || continue; n=$(basename "$f"); [ "$n" = LAYER ] && continue
    files=$((files + 1)); cmp -s "$f" "$TRUNK/$n" && same=$((same + 1))
  done
  total_before=$((total_before + files))
  if [ "$DO" = 1 ]; then msg=$("$bin" db trim 2>&1 | head -1); else msg="(dry run) would run: db trim"; fi
  left=0; for f in "$db"/*; do [ -f "$f" ] || continue; [ "$(basename "$f")" = LAYER ] || left=$((left + 1)); done
  total_after=$((total_after + left))
  echo "$a: $files file(s) in the layer, $same identical to the trunk -> $msg; now $left file(s)"
done
echo "total layer files: $total_before before, $total_after now$([ "$DO" = 1 ] || echo ' (dry run, nothing changed)')"
