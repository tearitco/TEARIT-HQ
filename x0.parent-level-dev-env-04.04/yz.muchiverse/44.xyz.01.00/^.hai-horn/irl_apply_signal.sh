#!/bin/bash
# irl_apply_signal.sh - apply IRL signal to Concept Bank spokes

set -euo pipefail

HAI_HORN_DIR="$(cd "$(dirname "$0")" && pwd)"
SIGNAL_FILE="${1:-$HAI_HORN_DIR/irl_signal.json}"
# no absolute path: the house root is two levels above this folder (^.hai-horn sits directly under it); BANK_DIR / HOUSE_ROOT may be set in the environment to override
HOUSE_ROOT="${HOUSE_ROOT:-$(cd "$HAI_HORN_DIR/.." && pwd)}"
BANK_DIR="${BANK_DIR:-$HOUSE_ROOT/&.widgits/concept-bank}"

if [ ! -f "$SIGNAL_FILE" ]; then
    echo "Error: Signal file not found: $SIGNAL_FILE" >&2
    exit 1
fi

# Extract delta from signal
DELTA=$(python3 -c "
import json, sys
with open('$SIGNAL_FILE') as f:
    data = json.load(f)
print(data.get('concept_bank_delta', 0))
")

echo "=== Applying IRL Signal ==="
echo "Delta: $DELTA"
echo "Halo win rate: $(python3 -c "import json; print(json.load(open('$SIGNAL_FILE'))['halo_win_rate'])")"
echo "Horn win rate: $(python3 -c "import json; print(json.load(open('$SIGNAL_FILE'))['horn_win_rate'])")"

# Spokes that can accept weight changes (not at max)
SPOKES=(
    "mass:force"
    "acceleration:motion"
    "velocity:motion"
    "kinetic_energy:energy"
)

for pair in "${SPOKES[@]}"; do
    SPOKE="${pair%:*}"
    MASTER="${pair#*:}"
    SPOKE_FILE="$BANK_DIR/data/spokes/${SPOKE}.pdl"
    
    if [ ! -f "$SPOKE_FILE" ]; then
        echo "Skipping $SPOKE (not found)"
        continue
    fi
    
    # Read current weight
    CURRENT_WEIGHT=$(grep -o 'WEIGHT=[0-9.]\+' "$SPOKE_FILE" | head -1 | cut -d= -f2)
    
    if [ -z "$CURRENT_WEIGHT" ]; then
        echo "Skipping $SPOKE (no weight found)"
        continue
    fi
    
    echo ""
    echo "Spoke: $SPOKE -> $MASTER"
    echo "  Current weight: $CURRENT_WEIGHT"
    
    # Compute new weight
    NEW_WEIGHT=$(python3 -c "
current = $CURRENT_WEIGHT
delta = $DELTA
new = current + delta
if new > 1.0: new = 1.0
if new < -1.0: new = -1.0
print('%.4f' % new)
")
    
    echo "  New weight: $NEW_WEIGHT"
    
    if [ "$(python3 -c "print($NEW_WEIGHT != $CURRENT_WEIGHT)")" = "True" ]; then
        # Update spoke file
        sed -i "s/WEIGHT=$CURRENT_WEIGHT/WEIGHT=$NEW_WEIGHT/" "$SPOKE_FILE"
        echo "  ✓ Updated"
    else
        echo "  - No change (at bounds)"
    fi
done

echo ""
echo "=== Updated spokes ==="
for pair in "${SPOKES[@]}"; do
    SPOKE="${pair%:*}"
    SPOKE_FILE="$BANK_DIR/data/spokes/${SPOKE}.pdl"
    if [ -f "$SPOKE_FILE" ]; then
        WEIGHT=$(grep -o 'WEIGHT=[0-9.]\+' "$SPOKE_FILE" | head -1 | cut -d= -f2)
        echo "  $SPOKE: $WEIGHT"
    fi
done