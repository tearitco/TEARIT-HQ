#!/bin/sh
# concept_mirror_rebuild.sh - regenerates each master's MIRROR TABLE
# ("who points at me") by scanning every spoke record's own forward
# slots. Per AI-TRACK-BRAINSTORM-QUESTIONS.md Q9 §9b: weights live in
# exactly ONE place (a spoke's own forward SLOT records) - a master's
# mirror table is a derived, regenerated index, NEVER a second
# authoritative copy. Running this tool is the only way the mirror
# block in data/masters/*.pdl should ever change; hand-editing it is
# exactly the "two independently-editable copies of the same fact" bug
# class Q9 9b names directly (the phymoji sprite-cache staleness bug).
#
# Usage: concept_mirror_rebuild.sh <concept_bank_dir>
set -u
BANK="${1:-}"
[ -n "$BANK" ] && [ -d "$BANK/data/masters" ] && [ -d "$BANK/data/spokes" ] || {
    echo "usage: concept_mirror_rebuild.sh <concept_bank_dir>  (expects data/masters + data/spokes under it)" >&2
    exit 2
}

TS="$(date '+%Y-%m-%d %H:%M:%S')"

for master_file in "$BANK"/data/masters/*.pdl; do
    [ -f "$master_file" ] || continue
    master_name="$(basename "$master_file" .pdl)"

    # collect every spoke slot that POINTS_TO=$master_name
    MIRROR_ROWS=""
    for spoke_file in "$BANK"/data/spokes/*.pdl; do
        [ -f "$spoke_file" ] || continue
        spoke_name="$(basename "$spoke_file" .pdl)"
        while IFS= read -r line; do
            case "$line" in
                *SLOT*"POINTS_TO=$master_name "*|*SLOT*"POINTS_TO=$master_name"$'\t'*) : ;;
                *SLOT*"POINTS_TO=$master_name") : ;;
                *) continue ;;
            esac
            weight="$(printf '%s' "$line" | sed -n 's/.*WEIGHT=\([-0-9.]*\).*/\1/p')"
            MIRROR_ROWS="${MIRROR_ROWS}MIRROR_FROM | $spoke_name | WEIGHT=${weight:-0}
"
        done < "$spoke_file"
    done

    # truncate the master file at "MIRROR_GENERATED_AT" (or EOF if
    # somehow absent) and rewrite it with the freshly regenerated block.
    tmp="$master_file.tmp.$$"
    awk -v marker="MIRROR_GENERATED_AT" '
        index($0, marker) { exit }
        { print }
    ' "$master_file" > "$tmp"
    {
        cat "$tmp"
        echo "MIRROR_GENERATED_AT | $TS"
        if [ -n "$MIRROR_ROWS" ]; then
            printf '%s' "$MIRROR_ROWS"
        else
            echo "# (no spoke currently points at this master)"
        fi
    } > "$master_file"
    rm -f "$tmp"
    echo "concept_mirror_rebuild.sh: rebuilt mirror for '$master_name'"
done
