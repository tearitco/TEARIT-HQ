#!/bin/sh
# quest_new.sh <title...> - add a quest from the board's text field: next id, folder from _TEMPLATE/QUEST.md, a row in quests/INDEX.md (status open, owner-created).
# QUESTS_DIR overrides the quests folder (tests use a scratch copy). The title is data: it is never put through a shell, '|' and newlines are stripped.
set -eu
Q="${QUESTS_DIR:-$(cd "$(dirname "$0")/.." && pwd)/quests}"
TITLE="$(printf '%s' "$*" | tr '\n\r|' '   ' | sed 's/^ *//; s/ *$//')"
[ -n "$TITLE" ] || { echo "quest_new: empty title" >&2; exit 1; }
TITLE="$(printf '%.140s' "$TITLE")"
# next id = highest Qnnn seen in the index, live folders or _deleted/, plus one (ids are never reused)
MAX="$( { grep -o '\[Q[0-9]*\]' "$Q/INDEX.md"; ls "$Q" "$Q/_deleted" 2>/dev/null | grep -o '^Q[0-9]*'; } | tr -d '[]Q' | sort -n | tail -1)"
MAX="$(printf '%s' "${MAX:-0}" | sed 's/^0*//')"; MAX="${MAX:-0}"   # strip leading zeros: 009 would be octal in $(( ))
ID="$(printf 'Q%03d' $(( MAX + 1 )))"
SLUG="$(printf '%s' "$TITLE" | tr 'A-Z' 'a-z' | tr -c 'a-z0-9' '-' | sed 's/--*/-/g; s/^-//; s/-$//' | cut -c1-40 | sed 's/-$//')"
DIR="$ID-${SLUG:-new}"
mkdir "$Q/$DIR"
awk -v id="$ID" -v t="$TITLE" -v d="$(date +%F)" '
  NR==1 { print "# " id " — " t; next }
  /^\| posted \|/ { print "| posted | " d " by owner (created from the board) |"; next }
  { print }' "$Q/_TEMPLATE/QUEST.md" > "$Q/$DIR/QUEST.md"
ROW="| [$ID]($DIR/QUEST.md) | $TITLE | - | - | open | - | - |"
LAST="$(grep -n '^| \[Q' "$Q/INDEX.md" | tail -1 | cut -d: -f1)"
if [ -n "$LAST" ]; then sed -i "${LAST}a\\
$ROW" "$Q/INDEX.md"; else printf '%s\n' "$ROW" >> "$Q/INDEX.md"; fi
NEXT="$(printf 'Q%03d' $(( MAX + 2 )))"
sed -i "s/^Next ids start at Q[0-9]*\./Next ids start at $NEXT./" "$Q/INDEX.md"
echo "created $ID ($DIR)"
