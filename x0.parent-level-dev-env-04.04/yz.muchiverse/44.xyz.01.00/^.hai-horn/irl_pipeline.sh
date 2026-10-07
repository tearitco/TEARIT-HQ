#!/bin/bash
# irl_pipeline.sh - complete IRL loop: prompts → HORN/HALO → grade → signal → apply → curriculum

set -euo pipefail

HAI_HORN_DIR="$(cd "$(dirname "$0")" && pwd)"

# Find house root
HOUSE_ROOT=""
test_dir="$HAI_HORN_DIR"
for i in {1..10}; do
    if [ -d "$test_dir/&.widgits/entity-cli/ops" ]; then
        HOUSE_ROOT="$test_dir"
        break
    fi
    if [ -d "$test_dir/&.widgits" ] && [ "$(basename "$test_dir")" = "44.xyz.01.00" ]; then
        HOUSE_ROOT="$test_dir"
        break
    fi
    test_dir="$(dirname "$test_dir")"
done

if [ -z "$HOUSE_ROOT" ]; then
    echo "ERROR: Cannot find house root"
    exit 1
fi

PROMPT_FILE="${1:-$HAI_HORN_DIR/irl_test_prompts.txt}"
CYCLES="${2:-1}"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║           IRL PIPELINE - Sprint 3                          ║"
echo "║   HORN vs HALO → Grade → Signal → Concept Bank → Curriculum║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "House root: $HOUSE_ROOT"
echo "Prompt file: $PROMPT_FILE"
echo "Cycles: $CYCLES"
echo ""

for cycle in $(seq 1 $CYCLES); do
    echo "═══ Cycle $cycle / $CYCLES ═══"
    
    # Step 1: Run IRL harness
    echo "→ Step 1: Running HORN vs HALO comparisons..."
    timeout 600 "$HAI_HORN_DIR/ops/irl_harness.+x" "$HOUSE_ROOT" "$HAI_HORN_DIR" "$PROMPT_FILE" "$HAI_HORN_DIR" 2>&1 | tail -20
    
    # Step 2: Compute signal
    echo "→ Step 2: Computing training signal..."
    "$HAI_HORN_DIR/ops/irl_signal.+x" "$HAI_HORN_DIR/irl_training_data.jsonl" "$HAI_HORN_DIR/irl_signal.json"
    
    # Step 3: Apply signal to Concept Bank
    echo "→ Step 3: Applying signal to Concept Bank..."
    "$HAI_HORN_DIR/irl_apply_signal.sh" "$HAI_HORN_DIR/irl_signal.json"
    
    # Step 4: Generate updated curriculum
    echo "→ Step 4: Generating updated curriculum..."
    "$HAI_HORN_DIR/ops/curricula_engine.+x" "$HOUSE_ROOT/&.widgits/concept-bank" -1 text
    
    echo ""
    echo "Cycle $cycle complete."
    echo ""
done

echo "═══ IRL Pipeline Complete ═══"
echo "Training data: $HAI_HORN_DIR/irl_training_data.jsonl"
echo "Signal: $HAI_HORN_DIR/irl_signal.json"
echo "Curriculum: Run '/curriculum' in HALO_CHAT to view"