#!/bin/bash

# Curriculum generator - produces human-readable curriculum from Concept Bank
# Usage: curricula_gen.sh <bank_dir> [grade] [format]

set -e

BANK_DIR="${1:?Usage: curricula_gen.sh <bank_dir> [grade] [format]}"
GRADE="${2:-all}"
FORMAT="${3:-text}"

ENGINE_BIN="$(dirname "$0")/ops/curricula_engine.+x"

if [ ! -f "$ENGINE_BIN" ]; then
    echo "Error: curricula_engine.+x not found at $ENGINE_BIN" >&2
    exit 1
fi

if [ "$FORMAT" = "json" ]; then
    "$ENGINE_BIN" "$BANK_DIR" "$GRADE"
    exit 0
fi

# Human-readable text format
json=$("$ENGINE_BIN" "$BANK_DIR" "$GRADE")

# Parse JSON with simple tools
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║           CONCEPT BANK CURRICULUM                          ║"
echo "╠══════════════════════════════════════════════════════════════╣"
echo "║  Bank: $BANK_DIR"
echo "║  Grade filter: $GRADE"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Simple JSON parsing for display
echo "$json" | sed 's/\[//g; s/\]//g; s/},/\n/g; s/{//g; s/}//g; s/"//g' | while IFS= read -r line; do
    if [ -z "$line" ] || [[ "$line" =~ ^[[:space:]]*$ ]]; then
        continue
    fi
    
    spoke=$(echo "$line" | sed -n 's/.*spoke: \([^,]*\).*/\1/p')
    master=$(echo "$line" | sed -n 's/.*master: \([^,]*\).*/\1/p')
    weight=$(echo "$line" | sed -n 's/.*weight: \([^,]*\).*/\1/p')
    grade=$(echo "$line" | sed -n 's/.*grade: \([^,]*\).*/\1/p')
    deps=$(echo "$line" | sed -n 's/.*depends_on: \[\([^]]*\)\].*/\1/p')
    
    if [ -n "$spoke" ]; then
        case $grade in
            0) grade_name="Preschool" ;;
            1) grade_name="Elementary" ;;
            2) grade_name="Middle School" ;;
            3) grade_name="High School" ;;
            4) grade_name="College" ;;
            5) grade_name="Graduate" ;;
            *) grade_name="Unknown" ;;
        esac
        
        printf "  ▸ %-30s → %-12s (weight: %.2f) [%s]\n" "$spoke" "$master" "$weight" "$grade_name"
        if [ -n "$deps" ] && [ "$deps" != "" ]; then
            printf "      Depends on: %s\n" "$deps"
        fi
    fi
done

echo ""
echo "Total items: $(echo "$json" | grep -c '"spoke"')"