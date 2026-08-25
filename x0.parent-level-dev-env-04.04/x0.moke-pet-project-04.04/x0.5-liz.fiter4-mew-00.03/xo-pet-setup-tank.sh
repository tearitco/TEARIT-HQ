#!/bin/bash
# xo-pet-setup-tank.sh - Scaffold the XO-PET enclosure and default entities.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
WORLD="world_tank_01"
MAP="map_enclosure"
PETS=("$@")

if [ ${#PETS[@]} -eq 0 ]; then
    PETS=("liz_bulb" "liz_char")
fi

TANK_ROOT="$PROJECT_ROOT/pieces/$WORLD/$MAP"

echo "Setting up XO-PET tank at $TANK_ROOT"
mkdir -p "$TANK_ROOT/food_pool" "$TANK_ROOT/rock_pool" "$PROJECT_ROOT/pieces/$WORLD/logs"

for PET in "${PETS[@]}"; do
    echo "Creating pet scaffold: $PET"
    mkdir -p "$TANK_ROOT/$PET/stomach" "$TANK_ROOT/$PET/ops" "$TANK_ROOT/$PET/memory"
    if [ ! -f "$TANK_ROOT/$PET/piece.pdl" ]; then
        cat > "$TANK_ROOT/$PET/piece.pdl" <<EOF
METHOD | scan   | pieces/$WORLD/$MAP/$PET/ops/scan.+x
METHOD | eat    | pieces/$WORLD/$MAP/$PET/ops/eat.+x
METHOD | breathe| pieces/$WORLD/$MAP/$PET/ops/breathe.+x
METHOD | rest   | pieces/$WORLD/$MAP/$PET/ops/rest.+x
EOF
    fi
done
