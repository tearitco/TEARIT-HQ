#!/bin/bash
# lt_pipeline.sh — W1 driver: plan -> fill (270m@LINUX) -> verify (1b@MAC)
#                 -> apply -> grade (deterministic) for one LT chapter.
# Orchestration only; all model work happens on LAN nodes (10.0.0.187 / .144).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
BOOK="${1:-solpen}"
CHAPTER="${2:-ch01}"

echo "=== W1 LT pipeline: book=$BOOK chapter=$CHAPTER ==="

# 1. build the new ops (warning-free bar, house standard)
echo "--- build ops ---"
cd "$SCRIPT_DIR"
mkdir -p ops/+x
for src in plan_cells fill_cell verify_cell apply_cell grade_chapter; do
    gcc -Wall -Wextra -O2 "ops/$src.c" -o "ops/+x/$src.+x"
    echo "  ok ops/+x/$src.+x"
done

# 2. plan cells
echo "--- plan_cells ---"
"$SCRIPT_DIR/ops/+x/plan_cells.+x" "$BOOK" "$CHAPTER"

# 3. fill + verify per cell (cell_01..cell_07, or however many plan made)
NCELLS=$(grep '^cells|' "$SCRIPT_DIR/canon/work/$BOOK/$CHAPTER/cells.manifest" | cut -d'|' -f2)
echo "--- generating $NCELLS cells (270m on 10.0.0.187) ---"
for i in $(seq 1 "$NCELLS"); do
    cid=$(printf "cell_%02d" "$i")
    echo "[$cid] fill (270m)..."
    "$SCRIPT_DIR/ops/+x/fill_cell.+x" "$BOOK" "$CHAPTER" "$cid"
    echo "[$cid] verify (1b on 10.0.0.144)..."
    "$SCRIPT_DIR/ops/+x/verify_cell.+x" "$BOOK" "$CHAPTER" "$cid" || true
done

# 4. assemble
echo "--- apply_cell ---"
"$SCRIPT_DIR/ops/+x/apply_cell.+x" "$BOOK" "$CHAPTER"

# 5. grade
echo "--- grade_chapter (deterministic vs gold) ---"
"$SCRIPT_DIR/ops/+x/grade_chapter.+x" "$BOOK" "$CHAPTER"

echo "=== done: $SCRIPT_DIR/canon/work/$BOOK/$CHAPTER/chapter.generated.txt ==="
