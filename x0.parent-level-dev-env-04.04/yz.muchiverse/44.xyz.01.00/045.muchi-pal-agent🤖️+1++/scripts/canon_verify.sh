#!/bin/bash
# canon_verify.sh — W0 store verification (K3-style: evidence + assertions)
# Re-derives the compendium-style character index from the ledger.
set -euo pipefail

AGENT="$(cd "$(dirname "$0")/.." && pwd)"
CANON="$AGENT/canon"
BOOK="solpen"
PASS=0; FAIL=0

check() { # check <label> <cond>
    if [ "$2" = "1" ]; then PASS=$((PASS+1)); echo "  [PASS] $1";
    else FAIL=$((FAIL+1)); echo "  [FAIL] $1"; fi
}

echo "=== W0 canon verify: book=$BOOK ==="

# 1. atom completeness
n_scenes=$(ls "$CANON"/source/$BOOK/scene_*.txt 2>/dev/null | wc -l)
check "40 screenplay scene atoms present (got $n_scenes)" "$([ "$n_scenes" -eq 40 ] && echo 1 || echo 0)"

n_lt=$(ls "$CANON"/lt/$BOOK/ch*.txt 2>/dev/null | wc -l)
check "21 LT chapter atoms present (got $n_lt)" "$([ "$n_lt" -eq 21 ] && echo 1 || echo 0)"

# 2. chapter continuity ch01..ch21 (gold set completeness, incl. ch16 from ooo/)
missing=""
for i in $(seq -w 1 21); do
    [ -f "$CANON/lt/$BOOK/ch$i.txt" ] || missing="$missing ch$i"
done
check "LT chapters ch01..ch21 contiguous (missing:$missing)" "$([ -z "$missing" ] && echo 1 || echo 0)"

# 3. zero-hit entities are a WARNING (documented coverage gaps), not a gate:
#    Elara/watcher birds = franchise-level (compendium), Lunaria/HALO-full-name/
#    Energy Blades/Nanobots/Bypass Key = only in _meta seeds, absent from bk0 text.
zero=""
for lex in entities.pdl places.pdl items.pdl; do
    while IFS='|' read -r id type name aliases note; do
        [ -n "$id" ] || continue
        case "$id" in \#*|"") continue ;; esac
        IFS=';' read -ra names <<< "$name;$aliases"
        for alias in "${names[@]}"; do
            [ -n "$alias" ] || continue
            if grep -qF -- "|$alias|hits=0|" "$CANON/ledger/entity_index.pdl"; then
                zero="$zero $alias"
            fi
        done
    done < "$CANON/lexicon/$lex"
done
if [ -n "$zero" ]; then
    echo "  [WARN] zero-hit aliases (expected, meta/franchise-only):$zero"
else
    echo "  [PASS] every alias appears in >=1 canon atom"
fi

# 4. index regenerable: row count matches manifest
rows=$(wc -l < "$CANON/ledger/entity_index.pdl")
manifest_rows=$(grep '^index_rows|' "$CANON/manifest.pdl" | cut -d'|' -f2)
check "entity_index has $manifest_rows rows (got $rows)" "$([ "$rows" = "$manifest_rows" ] && echo 1 || echo 0)"

# 5. re-derive compendium-style character index (from ledger, top by coverage)
echo
echo "=== derived compendium index (persons by canon coverage) ==="
while IFS='|' read -r id type alias hits_rest; do
    [ "$type" = "person" ] || continue
    hits="${hits_rest%%|*}"
    hits="${hits#hits=}"
    note=$(grep -m1 "^$id|person|" "$CANON/lexicon/entities.pdl" | cut -d'|' -f5 || true)
    printf '%s|%s|%s|%s\n' "$hits" "$id" "$alias" "${note:-}"
done < "$CANON/ledger/entity_index.pdl" \
    | sort -t'|' -k1 -rn -k2 \
    | while IFS='|' read -r hits id alias note; do
        printf '  %-24s alias=%-22s hits=%-3s %s\n' "$id" "$alias" "$hits" "$note"
      done

echo
echo "=== result: PASS=$PASS FAIL=$FAIL ==="
[ "$FAIL" -eq 0 ]
