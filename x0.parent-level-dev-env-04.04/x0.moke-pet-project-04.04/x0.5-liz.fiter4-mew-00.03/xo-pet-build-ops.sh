#!/bin/bash
# xo-pet-build-ops.sh - Compile XO-PET ops into +x/ for the active project bundle.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORLD="world_tank_01"
MAP="map_enclosure"
PROJECT_ROOT="$SCRIPT_DIR"

shopt -s nullglob

for PET_DIR in "$PROJECT_ROOT/pieces/$WORLD/$MAP"/*; do
    [ -d "$PET_DIR/ops" ] || continue
    PET_NAME="$(basename "$PET_DIR")"
    OPS_DIR="$PET_DIR/ops"
    OUT_DIR="$OPS_DIR/+x"

    echo "Compiling ops for $PET_NAME..."
    mkdir -p "$OUT_DIR"

    for src in "$OPS_DIR"/*.c; do
        [ -f "$src" ] || continue
        base_name="$(basename "$src" .c)"
        if gcc -o "$OUT_DIR/$base_name.+x" "$src"; then
            echo "  ✓ $base_name compiled"
        else
            echo "  ! $base_name failed"
        fi
    done
done
