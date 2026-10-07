#!/bin/sh
# quest_delete.sh <Qnnn> - take a quest off the board. REVERSIBLE: its folder moves to quests/_deleted/ and its row is saved to _deleted/<id>.row; nothing is erased.
# (The board asks "are you sure?" first: Backspace on a row -> confirm popup.) QUESTS_DIR overrides the quests folder (tests).
set -eu
Q="${QUESTS_DIR:-$(cd "$(dirname "$0")/.." && pwd)/quests}"
ID="${1:-}"
case "$ID" in Q[A-Za-z0-9]*) ;; *) echo "quest_delete: not a quest id: $ID" >&2; exit 1 ;; esac
case "$ID" in *[!A-Za-z0-9]*) echo "quest_delete: not a quest id: $ID" >&2; exit 1 ;; esac   # letters and digits only: it is used in a grep and a path
DIR="$(ls "$Q" | grep "^$ID-" | head -1)"
[ -n "$DIR" ] || { echo "quest_delete: no folder for $ID" >&2; exit 1; }
mkdir -p "$Q/_deleted"
grep "^| \[$ID\]" "$Q/INDEX.md" > "$Q/_deleted/$ID.row" || true
grep -v "^| \[$ID\]" "$Q/INDEX.md" > "$Q/INDEX.md.tmp" && mv "$Q/INDEX.md.tmp" "$Q/INDEX.md"
mv "$Q/$DIR" "$Q/_deleted/$DIR"
echo "deleted $ID (moved to _deleted/$DIR; row saved)"
