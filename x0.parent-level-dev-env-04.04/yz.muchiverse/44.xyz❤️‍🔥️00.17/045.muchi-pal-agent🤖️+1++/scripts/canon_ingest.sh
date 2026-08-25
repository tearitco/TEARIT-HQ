#!/bin/bash
# canon_ingest.sh — W0 canon store builder for the Soul Pen franchise (bk0 = Sol Pen)
# Deterministic, idempotent, ZERO model calls. Copies + normalizes + indexes.
# Layout under the agent project:
#   canon/source/<book>/   screenplay scene atoms (source canon, immutable)
#   canon/lt/<book>/       gold Living Testament chapters (scripture, immutable)
#   canon/lexicon/         curated entity/place/item rows (seeded from source files)
#   canon/ledger/          derived indexes (built by grep, regenerable)
#   canon/refs/            cross-reference tables
set -euo pipefail

AGENT="$(cd "$(dirname "$0")/.." && pwd)"
CANON="$AGENT/canon"

SP_ROOT="/home/no/Desktop/🧩️Piecemark-IT/中.SP_00.00/!.sp-inniϕ©+31/!.SP.all-writeez31-c0.5/0.Sol Pen]bk0-K-ARK-2.1"
LT_ROOT="/home/no/Desktop/🧩️Piecemark-IT/中.SP_00.00/!.sp-inniϕ©+31/!.SP_AS_BIBLEϕ©=LT-01.00/0.LT-SP"

BOOK="solpen"
NOW="$(date '+%Y-%m-%d %H:%M:%S')"

mkdir -p "$CANON/source/$BOOK" "$CANON/lt/$BOOK" "$CANON/lexicon" \
         "$CANON/ledger" "$CANON/refs" "$CANON/source/$BOOK/_meta"

# ---------------------------------------------------------------
# 1. SOURCE CANON: copy screenplay scenes (immutable atoms)
# ---------------------------------------------------------------
SRC_COUNT=0
for f in "$SP_ROOT"/0.scenes-K-ARK-3.0/scene_*.txt; do
    [ -f "$f" ] || continue
    cp "$f" "$CANON/source/$BOOK/$(basename "$f")"
    SRC_COUNT=$((SRC_COUNT + 1))
done

# provenance copies (character/world/outline seeds live here, read-only)
for meta in character_sheet.txt world_building.txt; do
    if [ -f "$SP_ROOT/$meta" ]; then cp "$SP_ROOT/$meta" "$CANON/source/$BOOK/_meta/$meta"; fi
done
cp "$SP_ROOT/0.scenes-K-ARK-3.0/Sol_Pen_Screenplay_Outline.txt" \
   "$CANON/source/$BOOK/_meta/Sol_Pen_Screenplay_Outline.txt" 2>/dev/null || true

# ---------------------------------------------------------------
# 2. GOLD LT: copy Living Testament chapters, normalized chNN.txt
# ---------------------------------------------------------------
LT_COUNT=0
for f in "$LT_ROOT"/0.LT-SP-ch*.txt; do
    [ -f "$f" ] || continue
    b="$(basename "$f")"
    n="$(echo "$b" | sed -E 's/^0\.LT-SP-ch([0-9]+)\.txt$/\1/')"
    cp "$f" "$CANON/lt/$BOOK/ch$(printf '%02d' "$n").txt"
    LT_COUNT=$((LT_COUNT + 1))
done
# ch16 only exists in ooo/ as "best" — include as ch16
if [ -f "$LT_ROOT/ooo/0.LT-SP-ch16=best.txt" ]; then
    cp "$LT_ROOT/ooo/0.LT-SP-ch16=best.txt" "$CANON/lt/$BOOK/ch16.txt"
    LT_COUNT=$((LT_COUNT + 1))
fi

# ---------------------------------------------------------------
# 3. LEXICON: curated seeds (entity/place/item rows, pdl-style)
#    format: id | type | display-name | alias-list(;sep) | note
# ---------------------------------------------------------------
cat > "$CANON/lexicon/entities.pdl" << 'EOE'
lucky-shepherd|person|Lucky Shepherd|Lucky|protagonist, tech billionaire, carries SolPen
astra|person|Astra|Astra|Lucky's inventor friend and confidante
sol|person|Sol|Sol;SSOL|ageless masked leader of lunar society, future Lucky
loona-twilight|person|Loona Twilight|Loona|leader of the moon crew
gem|person|Gem|Gem|disguised clone spy, later ally
agent-thompson|person|Agent Thompson|Thompson|ruthless CIA agent
kaito|person|Kaito|Kaito|leader of the Shadow Syndicate
black-queen|person|Black Queen|Black Queen|leader of Martian clone rebels
princess-megan8r|person|Princess Megan8r|Megan8r;Princess Megan|Black Queen's second-in-command
elara|person|Elara|Elara|rogue individual on Mars
carcazod|species|Carcazods|Carcazod|mysterious alien race threatening the Clean Zone
watcher-birds|species|watcher birds|watcher birds|adopters and sentinels of the first sun (compendium)
EOE

cat > "$CANON/lexicon/places.pdl" << 'EOE'
earth|place|Earth|Earth|starting point; technologically advanced, conflict-plagued
moon|place|The Moon|Moon;Lunaria|hidden lunar society led by Sol; Lunaria underground city
mars|place|Mars|Mars|clone rebellion site, desolate outpost
clean-zone|place|The Clean Zone|Clean Zone|region Sol tries to create, free from chaos
EOE

cat > "$CANON/lexicon/items.pdl" << 'EOE'
solpen|item|SolPen|SolPen|3D digital printer using clone souls as power
ssol|vehicle|SSOL|SSOL|Sol's massive interstellar mothership
halo-hud|item|HALO Bio Software HUD|HALO|holographic heads-up display
light-exo-suit|item|Light Exo Suit|exo suit|lightweight suit for enhanced strength and speed
universal-bypass-key|item|Universal Bypass Key|bypass key|bypasses electronic locks
energy-blades|weapon|Energy Blades|energy blade|melee weapons used by Princess Megan8r
nanobots|weapon|Nanobots|nanobots|microscopic robots disabling electronic systems
EOE

# ---------------------------------------------------------------
# 4. LEDGER: entity index (entity | display | file:hits list)
#    Deterministic, regenerable — grep over source + lt atoms.
# ---------------------------------------------------------------
INDEX="$CANON/ledger/entity_index.pdl"
> "$INDEX"

build_index() {
    local lex="$1"
    local type_tag="$2"
    while IFS='|' read -r id type name aliases note; do
        [ -n "$id" ] || continue
        case "$id" in \#*|"") continue ;; esac
        names="$name;$aliases"
        IFS=';' read -ra name_arr <<< "$names"
        for n in "${name_arr[@]}"; do
            [ -n "$n" ] || continue
            hits=""
            total=0
            for canon_file in "$CANON"/source/"$BOOK"/*.txt "$CANON"/lt/"$BOOK"/*.txt; do
                if grep -qF -- "$n" "$canon_file" 2>/dev/null; then
                    hits="$hits $(basename "$canon_file")"
                    total=$((total + 1))
                fi
            done
            printf '%s|%s|%s|hits=%d|%s\n' "$id" "$type_tag" "$n" "$total" "$hits"
        done
    done < "$lex"
}

build_index "$CANON/lexicon/entities.pdl" "person" >> "$INDEX"
build_index "$CANON/lexicon/places.pdl"    "place"  >> "$INDEX"
build_index "$CANON/lexicon/items.pdl"     "item"   >> "$INDEX"

# sort + dedupe by id|alias
sort -u "$INDEX" -o "$INDEX"

# ---------------------------------------------------------------
# 5. MANIFEST (provenance + counts, regenerated every run)
# ---------------------------------------------------------------
cat > "$CANON/manifest.pdl" << EOF
# canon store manifest — auto-generated $NOW by scripts/canon_ingest.sh
book|$BOOK|Book of the Soul Pen
source_root|$SP_ROOT
lt_root|$LT_ROOT
scene_count|$SRC_COUNT
lt_chapter_count|$LT_COUNT
lexicon_entities|$(grep -c . "$CANON/lexicon/entities.pdl" || true)
lexicon_places|$(grep -c . "$CANON/lexicon/places.pdl" || true)
lexicon_items|$(grep -c . "$CANON/lexicon/items.pdl" || true)
index_rows|$(wc -l < "$INDEX")
EOF

echo "canon ingest complete: scenes=$SRC_COUNT lt_chapters=$LT_COUNT index_rows=$(wc -l < "$INDEX")"
