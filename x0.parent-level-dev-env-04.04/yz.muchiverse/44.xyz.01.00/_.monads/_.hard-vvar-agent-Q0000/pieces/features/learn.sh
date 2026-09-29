#!/bin/bash
# learn.sh - v1 learn op. Appends a fact to the corpus and to the ledger.
# Usage: learn.sh "<fact>"
. "$(cd "$(dirname "$0")/../brain" && pwd)/oplib.sh"

FACT="$*"
[ -z "$FACT" ] && FACT="(nothing to learn)"

CORPUS="$BRAIN_DIR/corpus/learned.txt"
printf '%s\n' "$FACT" >> "$CORPUS"
ledger_append "Learn" "learned: $FACT" "learn.sh"
echo "Learn: recorded '$FACT' (corpus now $(wc -l < "$CORPUS") entries)"
