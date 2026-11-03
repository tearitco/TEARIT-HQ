#!/bin/bash
# Proverbs 31 daily verse - day of month selects the verse (1-31); English (KJV) + Chinese (CUV), both public domain.
# The verses are DATA: ../../data/verses_proverbs31.pdl (V | day | English | Chinese). PROVERBS_DAY=<1-31> overrides today (testing).

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# house root = 9 levels up (page_1/branches/<this> -> pages -> event_pkg -> reader -> pieces -> _.book-stack -> _.monads -> house); it was 7, which landed on _.book-stack,
# so the show-text binary was never found and the verse only went to stdout (no window).
HOUSE="$(cd "$SCRIPT_DIR/../../../../../../../../.." && pwd)"
PACKAGE_DIR="${PACKAGE_DIR:-$HOUSE/_.monads/_.book-stack/entities/book-stack}"
DATA="$SCRIPT_DIR/../../data/verses_proverbs31.pdl"

DAY="${PROVERBS_DAY:-$(date +%d)}"
DAY=$((10#$DAY))  # strip leading zero
if [ "$DAY" -lt 1 ] || [ "$DAY" -gt 31 ]; then DAY=1; fi

# one row, split on " | " (awk with a literal separator; verse text has no pipe)
ROW=$(awk -F' [|] ' -v d="$DAY" '$1 == "V" && $2 == d {print $3 "\t" $4; exit}' "$DATA" 2>/dev/null)
VERSE_EN="${ROW%%$'\t'*}"; VERSE_ZH="${ROW#*$'\t'}"
if [ -z "$ROW" ] || [ -z "$VERSE_EN" ]; then echo "proverbs31: no verse for day $DAY in $DATA" >&2; exit 1; fi

TMP=$(mktemp --suffix=.txt 2>/dev/null || echo /tmp/proverbs31_$$.txt)
# wrap by CHARACTER, not byte: byte-wise fold cut Chinese characters in half
{ printf "Proverbs 31:%s\n" "$DAY"; printf "%s\n" "$VERSE_EN" | fold -s -w 70; printf "\n箴言 31:%s\n" "$DAY"; printf "%s\n" "$VERSE_ZH" | LC_ALL=C.UTF-8 sed -E 's/(.{1,24})/\1\n/g' | sed '/^$/d'; } > "$TMP"

SHOW="$HOUSE/&.widgits/tile-picker/ops/+x/khtpm_show_text.+x"
if [ -x "$SHOW" ]; then
    "$SHOW" "$PACKAGE_DIR" "$TMP"
else
    cat "$TMP"
fi
