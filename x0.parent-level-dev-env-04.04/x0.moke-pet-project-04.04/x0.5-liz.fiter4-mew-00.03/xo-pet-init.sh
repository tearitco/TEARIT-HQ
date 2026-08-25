#!/bin/bash
# xo-pet-init.sh - Initialize the XO-PET tank for either autonomous or controller mode.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORLD="world_tank_01"
MAP="map_enclosure"
PROJECT_ROOT="$SCRIPT_DIR"
TANK_ROOT="$PROJECT_ROOT/pieces/$WORLD/$MAP"

bash "$SCRIPT_DIR/xo-pet-setup-tank.sh" "$@"

echo "Initializing XO-PET world state"
echo "epoch=1" > "$PROJECT_ROOT/pieces/$WORLD/state.txt"
echo "status=active" >> "$PROJECT_ROOT/pieces/$WORLD/state.txt"

for PET_DIR in "$TANK_ROOT"/*; do
    [ -d "$PET_DIR" ] || continue
    [ -d "$PET_DIR/stomach" ] || continue
    PET_NAME="$(basename "$PET_DIR")"

    rm -rf "$PET_DIR/stomach"/*
    mkdir -p "$PET_DIR/memory"
    echo "type | lizard" > "$PET_DIR/state.txt"
    echo "name | $PET_NAME" >> "$PET_DIR/state.txt"
    echo "hp=10" > "$PET_DIR/memory/stats.txt"
    echo "hunger=0" >> "$PET_DIR/memory/stats.txt"
done

echo "XO-PET initialization complete."
